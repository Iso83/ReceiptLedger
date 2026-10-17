#include "Layout.h"

#include <set>

namespace OCR = ReceiptLedger::Engine::Ocr;

namespace ReceiptLedger::Internal::Receipt {

std::vector<Row> Layout::rows(const OCR::Result& source) {
    std::set<std::string> ids;
    for (const auto& region : source.regions) {
        if (region.id.empty() || !ids.insert(region.id).second)
            throw std::invalid_argument("interpretation: empty or duplicate region ID");
    }

    for (const auto& region : source.regions) {
        if (!region.parentId.empty() && (!ids.contains(region.parentId) || region.parentId == region.id))
            throw std::invalid_argument("interpretation: unknown parent region " + region.parentId);
    }

    for (const auto& field : source.fieldReadings) {
        const auto parent = std::ranges::find(source.regions, field.parentId, &OCR::Region::id);
        if (field.id.empty() || !ids.insert(field.id).second || parent == source.regions.end() ||
            parent->level != OCR::Level::Line)
            throw std::invalid_argument("interpretation: invalid field reading reference");
    }

    // Tesseract's line hierarchy is primary: a tall noisy word must not absorb the next item row.
    std::vector<Row> lineRows;
    for (const auto& line : source.regions) {
        if (line.level != OCR::Level::Line)
            continue;

        Row row;
        row.text = trim(line.text);
        row.regions = {line.id};
        row.top = line.top;
        row.bottom = line.bottom;
        row.uncertain = !line.confidence || *line.confidence < 85.0F;
        for (const auto& word : source.regions) {
            if (word.level == OCR::Level::Word && word.parentId == line.id) {
                row.words.push_back(word);
                row.uncertain |= !word.confidence || *word.confidence < 85.0F;
            }
        }
        for (const auto& symbol : source.regions) {
            if (symbol.level == OCR::Level::Symbol &&
                std::ranges::any_of(row.words, [&](const auto& word) { return symbol.parentId == word.id; }))
                row.symbols.push_back(symbol);
        }
        for (const auto& field : source.fieldReadings) {
            if (field.parentId == line.id)
                row.fields.push_back(field);
        }
        row.text = std::regex_replace(row.text, std::regex(R"(([0-9]+[,.])\s+([0-9]{2})\b)"), "$1$2");
        lineRows.push_back(std::move(row));
    }
    if (!lineRows.empty()) {
        std::ranges::stable_sort(lineRows, [](const auto& left, const auto& right) { return left.top < right.top; });
        return lineRows;
    }

    std::vector<const OCR::Region*> selected;
    const bool words =
        std::ranges::any_of(source.regions, [](const auto& region) { return region.level == OCR::Level::Word; });
    for (const auto& region : source.regions) {
        if (region.level == (words ? OCR ::Level::Word : OCR::Level::Line))
            selected.push_back(&region);
    }
    std::ranges::stable_sort(selected, [](const auto* left, const auto* right) {
        return left->top < right->top || (left->top == right->top && left->left < right->left);
    });

    std::vector<std::vector<const OCR::Region*>> groups;
    for (const auto* region : selected) {
        if (!words || groups.empty()) {
            groups.push_back({region});
            continue;
        }

        const auto* previous = groups.back().front();
        const int overlap = std::min(previous->bottom, region->bottom) - std::max(previous->top, region->top);
        const int height = std::min(previous->bottom - previous->top, region->bottom - region->top);
        if (overlap > 0 && overlap * 2 >= height &&
            std::abs((previous->top + previous->bottom) - (region->top + region->bottom)) <= height)
            groups.back().push_back(region);
        else
            groups.push_back({region});
    }

    std::vector<Row> result;
    for (auto& group : groups) {
        std::ranges::stable_sort(group, [](const auto* left, const auto* right) { return left->left < right->left; });
        Row row;
        row.top = group.front()->top;
        row.bottom = group.front()->bottom;
        for (const auto* region : group) {
            if (!row.text.empty())
                row.text += ' ';

            row.text += trim(region->text);
            if (region->level == OCR::Level::Word)
                row.words.push_back(*region);
            row.regions.push_back(region->id);
            row.top = std::min(row.top, region->top);
            row.bottom = std::max(row.bottom, region->bottom);
            row.uncertain |= !region->confidence || *region->confidence < 85.0F;
        }
        row.text = std::regex_replace(row.text, std::regex(R"(([0-9]+[,.])\s+([0-9]{2})\b)"), "$1$2");
        result.push_back(std::move(row));
    }

    return result;
}

std::string Layout::columnText(const Row& row, int left, int right) {
    std::string text;
    for (const auto& word : row.words) {
        if (word.right <= left || word.left >= right)
            continue;

        std::string part;
        if (word.left >= left && word.right <= right)
            part = word.text;
        else {
            for (const auto& symbol : row.symbols) {
                if (symbol.parentId == word.id && symbol.left >= left && symbol.right <= right)
                    part += symbol.text;
            }
        }

        if (!part.empty())
            text += (text.empty() ? "" : " ") + part;
    }

    return text;
}

} // namespace ReceiptLedger::Internal::Receipt
