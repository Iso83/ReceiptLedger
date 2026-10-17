#include "Columns.h"

#include <opencv2/core/mat.hpp>
#include <regex>

namespace OCR = ReceiptLedger::Engine::Ocr;

namespace ReceiptLedger::Internal::Ocr {

void Columns::read(OCR::Engine& engine, const cv::Mat& image, OCR::Result& result) {
    using OCR::Level;

    const std::regex amount(R"(^[+-]?[0-9]+[,.][0-9]{2}$)");
    const std::regex multiplication(R"(^\s*[0-9]+\s*[xX*])");
    const std::regex separatedCurrency(R"(^\s*[0-9]+\s*[xX*]\s*[0-9]+[,.][0-9]{2}\s*[^0-9\s]+\s*$)");
    std::vector<const OCR::Region*> lines;
    int stop = image.rows;
    for (const auto& region : result.regions) {
        if (region.level != Level::Line)
            continue;

        lines.push_back(&region);
        std::string upper = region.text;
        std::ranges::transform(upper, upper.begin(), [](unsigned char character) { return std::toupper(character); });
        if (upper.find("TE BETALEN") != std::string::npos || upper.find("KLANTTICKET") != std::string::npos ||
            upper.find("BETALING") != std::string::npos || upper.find("POI:") != std::string::npos)
            stop = std::min(stop, region.top);
    }

    const std::regex taxRow(
        R"(^\s*([A-Z0-9])?\s*[0-9]+(?:[,.][0-9]+)?\s*%\s+[0-9]+[,.][0-9]+\s+[0-9]+[,.][0-9]+\s+[0-9]+[,.][0-9]+\s*$)");
    for (const auto* line : lines) {
        if (!std::regex_match(line->text, taxRow)) {
            if (line->text.find("BTW") != std::string::npos && line->text.find('%') != std::string::npos)
                result.fieldReadings.push_back(
                    engine.readField(image, std::max(0, line->left - 8), std::max(0, line->top - 8),
                                     std::min(image.cols, line->right + 8), std::min(image.rows, line->bottom + 8),
                                     line->id, "tax-summary"));

            continue;
        }

        std::vector<const OCR::Region*> values;
        for (const auto& word : result.regions) {
            if (word.level == Level::Word && word.parentId == line->id && std::regex_match(word.text, amount))
                values.push_back(&word);
        }
        std::ranges::sort(values, [](const auto* left, const auto* right) { return left->left < right->left; });
        if (values.size() != 3)
            continue;

        const std::string purposes[] = {"tax-net", "tax-amount", "tax-gross"};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const auto& word = *values[index];
            result.fieldReadings.push_back(engine.readField(
                image, std::max(0, word.left - (word.bottom - word.top) / 4),
                std::max(0, word.top - (word.bottom - word.top) / 4),
                std::min(image.cols, word.right + (word.bottom - word.top) / 4),
                std::min(image.rows, word.bottom + (word.bottom - word.top) / 4), line->id, purposes[index]));
        }
    }

    std::vector<const OCR::Region*> candidates;
    std::vector<int> widths;
    std::vector<int> multiplicationEnds;
    for (const auto& word : result.regions) {
        if (word.level == Level::Symbol && word.bottom > word.top &&
            word.right - word.left > (word.bottom - word.top) / 4 &&
            std::ranges::all_of(word.text, [](unsigned char c) { return std::isdigit(c); }))
            widths.push_back(word.right - word.left);

        if (word.level == Level::Word && word.left > image.cols / 2 && word.top < stop &&
            std::regex_match(word.text, amount))
            candidates.push_back(&word);
    }
    if (widths.empty() || candidates.empty())
        return;

    std::ranges::sort(widths);
    const int characterWidth = std::max(2, widths[widths.size() / 2]);
    std::vector<const OCR::Region*> column;
    for (const auto* candidate : candidates) {
        std::vector<const OCR::Region*> aligned;
        for (const auto* other : candidates) {
            if (std::abs(candidate->right - other->right) <= characterWidth)
                aligned.push_back(other);
        }
        if (aligned.size() > column.size() ||
            (aligned.size() == column.size() && !column.empty() && candidate->right > column.front()->right))
            column = std::move(aligned);
    }

    // Require repeated observed alignment. No page-width percentage decides a monetary boundary.
    if (column.size() < 2)
        return;

    std::vector<int> ends;
    int left = image.cols;
    for (const auto* word : column) {
        ends.push_back(word->right);
        left = std::min(left, word->left);
    }
    std::ranges::sort(ends);
    const int end = ends[ends.size() / 2];
    left = std::max(0, left - 3 * characterWidth);
    const int right = std::min(image.cols, end + std::max(3, characterWidth / 2));

    std::vector<const OCR::Region*> secondary;
    for (const auto* candidate : candidates) {
        if (candidate->right >= end - 3 * characterWidth)
            continue;

        std::vector<const OCR::Region*> aligned;
        for (const auto* other : candidates) {
            if (std::abs(candidate->right - other->right) <= characterWidth)
                aligned.push_back(other);
        }
        if (aligned.size() > secondary.size())
            secondary = std::move(aligned);
    }

    int priceLeft = image.cols;
    int priceRight = 0;
    if (secondary.size() >= 2) {
        for (const auto* word : secondary) {
            priceLeft = std::min(priceLeft, word->left);
            priceRight = std::max(priceRight, word->right);
        }
    }
    for (const auto* line : lines) {
        if (line->top >= stop || !std::regex_match(line->text, separatedCurrency))
            continue;

        for (const auto& word : result.regions) {
            if (word.level != Level::Word || word.parentId != line->id)
                continue;

            for (const auto& symbol : result.regions) {
                if (symbol.level == Level::Symbol && symbol.parentId == word.id &&
                    std::ranges::all_of(symbol.text, [](unsigned char c) { return std::isdigit(c); }))
                    multiplicationEnds.push_back(symbol.right);
            }
        }
    }

    const int multiplicationEnd = multiplicationEnds.empty() ? -1 : *std::ranges::max_element(multiplicationEnds);
    for (const auto* line : lines) {
        if (line->top >= stop)
            continue;

        if (multiplicationEnd > 0 && std::regex_search(line->text, multiplication)) {
            result.fieldReadings.push_back(
                engine.readField(image, std::max(0, line->left - 2 * characterWidth), std::max(0, line->top - 4),
                                 std::min(image.cols, multiplicationEnd + std::max(4, characterWidth / 3)),
                                 std::min(image.rows, line->bottom + 4), line->id, "multiplication"));
            continue;
        }

        const OCR::Region* anchor = nullptr;
        for (const auto& word : result.regions) {
            if (word.level == Level::Word && word.parentId == line->id && word.left < right && word.right > left &&
                std::ranges::any_of(word.text, [](unsigned char c) { return std::isdigit(c); }))
                anchor = &word;
        }
        if (!anchor)
            continue;

        const int top = std::max(0, anchor->top - 4);
        const int bottom = std::min(image.rows, anchor->bottom + 4);
        result.fieldReadings.push_back(engine.readField(image, left, top, right, bottom, line->id, "amount"));
        if (priceRight) {
            const OCR::Region* price = nullptr;
            for (const auto& word : result.regions) {
                if (word.level == Level::Word && word.parentId == line->id && word.left < priceRight &&
                    word.right > priceLeft)
                    price = &word;
            }
            if (price) {
                const auto position = std::ranges::find(lines, line);
                if (position != lines.begin()) {
                    const auto* previous = *(position - 1);
                    if (previous->bottom <= line->top + characterWidth && previous->top < line->top &&
                        previous->right > previous->left && priceLeft - characterWidth > previous->left &&
                        !std::regex_search(previous->text, std::regex(R"([0-9]+[,.][0-9]+)")) &&
                        !std::regex_search(previous->text, std::regex(R"(^[^a-zA-Z0-9]*[0-9]+\s+)")))
                        result.fieldReadings.push_back(
                            engine.readField(image, std::max(0, previous->left - characterWidth),
                                             std::max(0, previous->top - 4), priceLeft - 2 * characterWidth,
                                             std::min(image.rows, previous->bottom + 4), previous->id, "description"));
                }
                result.fieldReadings.push_back(
                    engine.readField(image, std::max(0, priceLeft - characterWidth), std::max(0, price->top - 4),
                                     priceRight + std::max(3, characterWidth / 3),
                                     std::min(image.rows, price->bottom + 4), line->id, "unit-price"));
            }
        }

        const int codeLeft = end + 3 * characterWidth;
        const int codeRight = std::min(image.cols, end + 5 * characterWidth);
        if (codeLeft < codeRight)
            result.fieldReadings.push_back(
                engine.readField(image, codeLeft, top, codeRight, bottom, line->id, "marker"));
    }
}

} // namespace ReceiptLedger::Internal::Ocr
