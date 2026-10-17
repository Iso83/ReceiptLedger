#include "Application.h"

#include "Report.h"

#include <ReceiptLedger/engine/receipt/Interpreter.h>
#include <fpdfview.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <set>

namespace OCR = ReceiptLedger::Engine::Ocr;

namespace ReceiptLedger::App::PDF2CSV {

class PdfLibrary {
public:
    PdfLibrary() {
        FPDF_InitLibrary();
    }
    ~PdfLibrary() {
        FPDF_DestroyLibrary();
    }
};

struct DocumentCloser {
    void operator()(void* document) const {
        FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(document));
    }
};
struct PageCloser {
    void operator()(void* page) const {
        FPDF_ClosePage(static_cast<FPDF_PAGE>(page));
    }
};
struct BitmapCloser {
    void operator()(void* bitmap) const {
        FPDFBitmap_Destroy(static_cast<FPDF_BITMAP>(bitmap));
    }
};

void Application::prepareOutput(const std::filesystem::path& output) {
    if (std::filesystem::exists(output) && !std::filesystem::is_directory(output))
        throw std::runtime_error("output: path is not a directory: " + output.string());

    std::filesystem::create_directories(output);
    if (!std::filesystem::is_directory(output))
        throw std::runtime_error("output: path is not a directory: " + output.string());

    // ocr.json is the commit marker. Invalidate it before any guard can reject a rerun.
    for (const auto* report : {"ocr.json", "ocr.csv", "ocr-regions.md", "items.csv", "receipts.csv"}) {
        std::error_code error;
        std::filesystem::remove(output / report, error);
        if (error)
            throw std::runtime_error("output: could not remove stale report " + (output / report).string() + ": " +
                                     error.message());
    }
}

std::vector<std::filesystem::path> Application::expandInputs(const Options& options) {
    auto requested = options.inputs;
    if (requested.empty())
        requested.push_back(options.input);

    // Match the filename, never directory separators. Windows shells pass these patterns literally.
    const auto matches = [](std::string pattern, std::string name) {
        const auto lower = [](unsigned char c) { return std::tolower(c); };
        std::ranges::transform(pattern, pattern.begin(), lower);
        std::ranges::transform(name, name.begin(), lower);
        std::size_t p = 0, n = 0, star = std::string::npos, retry = 0;
        while (n < name.size()) {
            if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
                ++p;
                ++n;
            } else if (p < pattern.size() && pattern[p] == '*') {
                star = p++;
                retry = n;
            } else if (star != std::string::npos) {
                p = star + 1;
                n = ++retry;
            } else
                return false;
        }

        while (p < pattern.size() && pattern[p] == '*')
            ++p;

        return p == pattern.size();
    };

    std::vector<std::filesystem::path> inputs;
    for (const auto& input : requested) {
        const bool directory = std::filesystem::is_directory(input);
        const auto pattern = directory ? std::string("*.pdf") : input.filename().string();
        if (!directory && pattern.find_first_of("*?") == std::string::npos) {
            inputs.push_back(input);
            continue;
        }

        auto parent = directory ? input : input.parent_path();
        if (parent.empty())
            parent = ".";

        if (parent.string().find_first_of("*?") != std::string::npos)
            throw std::invalid_argument("input: wildcards are supported only in filenames: " + input.string());

        std::vector<std::filesystem::path> found;
        if (std::filesystem::is_directory(parent)) {
            for (const auto& entry : std::filesystem::directory_iterator(parent)) {
                if (entry.is_regular_file() && matches(pattern, entry.path().filename().string()) &&
                    matches("*.pdf", entry.path().filename().string()))
                    found.push_back(entry.path());
            }
        }
        if (found.empty())
            throw std::invalid_argument("input: no PDF files found for " + input.string());

        std::ranges::sort(found);
        inputs.insert(inputs.end(), found.begin(), found.end());
    }

    return inputs;
}

cv::Mat Application::render(void* document, int index, int dpi) {
    std::unique_ptr<void, PageCloser> page(FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document), index));
    if (!page)
        throw std::runtime_error("render: PDFium could not load page " + std::to_string(index + 1));

    const double scale = static_cast<double>(dpi) / 72.0;
    const int width =
        std::max(1, static_cast<int>(std::lround(FPDF_GetPageWidth(static_cast<FPDF_PAGE>(page.get())) * scale)));
    const int height =
        std::max(1, static_cast<int>(std::lround(FPDF_GetPageHeight(static_cast<FPDF_PAGE>(page.get())) * scale)));
    std::unique_ptr<void, BitmapCloser> bitmap(FPDFBitmap_Create(width, height, 1));
    if (!bitmap)
        throw std::runtime_error("render: PDFium could not allocate page bitmap");

    FPDFBitmap_FillRect(static_cast<FPDF_BITMAP>(bitmap.get()), 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(static_cast<FPDF_BITMAP>(bitmap.get()), static_cast<FPDF_PAGE>(page.get()), 0, 0, width,
                          height, 0, FPDF_ANNOT | FPDF_LCD_TEXT);
    cv::Mat bgra(height, width, CV_8UC4, FPDFBitmap_GetBuffer(static_cast<FPDF_BITMAP>(bitmap.get())),
                 FPDFBitmap_GetStride(static_cast<FPDF_BITMAP>(bitmap.get())));
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

cv::Mat Application::inputImage(const cv::Mat& renderImage) {
    cv::Mat gray;
    cv::cvtColor(renderImage, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat Application::createOverlay(const cv::Mat& source, const ReceiptLedger::Engine::Ocr::Result& result,
                                   ReceiptLedger::Engine::Ocr::Level level, float low, float high) {
    const auto color = [low, high](const std::optional<float>& confidence) {
        if (!confidence)
            return cv::Scalar{128, 128, 128};

        if (*confidence >= high)
            return cv::Scalar{0, 170, 0};

        if (*confidence >= low)
            return cv::Scalar{0, 165, 255};

        return cv::Scalar{0, 0, 220};
    };
    const auto symbolCount = std::ranges::count_if(
        result.regions, [](const auto& region) { return region.level == ReceiptLedger::Engine::Ocr::Level::Symbol; });
    constexpr int panelColumnWidth = 112;
    const int panelRows = std::max(1, (source.rows - 64) / 16);
    const int panelColumns = level == ReceiptLedger::Engine::Ocr::Level::Symbol
                                 ? std::max(1, static_cast<int>((symbolCount + panelRows - 1) / panelRows))
                                 : 1;
    const int panelWidth = std::max(620, panelColumns * panelColumnWidth + 16);
    cv::Mat output(source.rows, source.cols + panelWidth, source.type(), cv::Scalar::all(255));
    source.copyTo(output(cv::Rect(0, 0, source.cols, source.rows)));
    if (level == ReceiptLedger::Engine::Ocr::Level::Word) {
        for (std::size_t index = 0; index < result.regions.size(); ++index) {
            const auto& region = result.regions[index];
            if (region.level != ReceiptLedger::Engine::Ocr::Level::Line)
                continue;

            cv::rectangle(output, {region.left, region.top}, {region.right, region.bottom}, {180, 80, 180}, 1);
            cv::putText(output, regionLabel(result, index), {region.left, std::max(48, region.top - 18)},
                        cv::FONT_HERSHEY_SIMPLEX, 0.42, {180, 80, 180}, 1, cv::LINE_AA);
        }
    }

    for (std::size_t index = 0; index < result.regions.size(); ++index) {
        const auto& region = result.regions[index];
        if (region.level != level)
            continue;

        const cv::Scalar stroke = color(region.confidence);
        const std::string label = regionLabel(result, index);
        cv::rectangle(output, {region.left, region.top}, {region.right, region.bottom}, stroke, 2);
        if (level != ReceiptLedger::Engine::Ocr::Level::Symbol)
            cv::putText(output, label, {region.left, std::max(12, region.top - 4)}, cv::FONT_HERSHEY_SIMPLEX, 0.42,
                        stroke, 1, cv::LINE_AA);
    }
    if (level == ReceiptLedger::Engine::Ocr::Level::Symbol) {
        int symbolIndex = 0;
        for (std::size_t index = 0; index < result.regions.size(); ++index) {
            const auto& region = result.regions[index];
            if (region.level != ReceiptLedger::Engine::Ocr::Level::Symbol)
                continue;

            const int column = symbolIndex / panelRows;
            const int row = symbolIndex % panelRows;
            cv::putText(output, regionLabel(result, index) + ":" + region.text,
                        {source.cols + 8 + column * panelColumnWidth, 60 + row * 16}, cv::FONT_HERSHEY_SIMPLEX, 0.38,
                        color(region.confidence), 1, cv::LINE_AA);
            ++symbolIndex;
        }
    }
    std::ostringstream legend;
    legend << "green >= " << high << " | orange >= " << low << " | red < " << low << " | gray unavailable";
    cv::putText(output, legend.str(), {source.cols + 8, 24}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1, cv::LINE_AA);
    cv::putText(output, "source coordinates unchanged; index label:text", {source.cols + 8, 44},
                cv::FONT_HERSHEY_SIMPLEX, 0.42, {0, 0, 0}, 1, cv::LINE_AA);
    return output;
}

int Application::run(const Options& options) {
    prepareOutput(options.output);
    const auto inputs = expandInputs(options);

    Options checked = options;
    checked.input = inputs.front();
    validate(checked);
    PdfLibrary library;
    OCR::Engine engine(options.tessdata, options.language, options.dpi, options.psm);
    std::vector<PageResult> pages;
    bool failed = false;
    std::set<std::string> seen;
    for (std::size_t inputIndex = 0; inputIndex < inputs.size(); ++inputIndex) {
        const auto& inputPath = inputs[inputIndex];
        const auto identity = documentId(inputPath);
        if (!seen.insert(identity).second)
            throw std::invalid_argument("input: duplicate document: " + inputPath.string());

        std::unique_ptr<void, DocumentCloser> document(FPDF_LoadDocument(inputPath.string().c_str(), nullptr));
        if (!document) {
            failed = true;
            pages.push_back(
                {0, std::nullopt, "failed: PDFium could not open " + inputPath.string(), {}, inputPath, identity});
            continue;
        }

        const int pageCount = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(document.get()));
        if (pageCount <= 0) {
            failed = true;
            pages.push_back({0, std::nullopt, "failed: document has no pages", {}, inputPath, identity});
        }
        for (int index = 0; index < pageCount; ++index) {
            try {
                std::ostringstream numberStream;
                numberStream << std::setw(4) << std::setfill('0') << index + 1;
                const auto number = numberStream.str();
                const auto pageId = identity + "-page-" + number;
                const auto prefix =
                    inputs.size() == 1 ? std::string{} : "document-" + std::to_string(inputIndex + 1) + "-";
                const cv::Mat rendered = render(document.get(), index, options.dpi);
                const cv::Mat input = inputImage(rendered);
                auto recognition = engine.recognizeReceipt(input, pageId);
                auto result = std::move(recognition.source);
                const auto renderName = prefix + "page-" + number + "-render.png";
                if (options.debug) {
                    if (!cv::imwrite((options.output / renderName).string(), rendered) ||
                        !cv::imwrite((options.output / (prefix + "page-" + number + "-input.png")).string(), input) ||
                        !cv::imwrite((options.output / (prefix + "page-" + number + "-symbols.png")).string(),
                                     createOverlay(rendered, result, Engine::Ocr::Level::Symbol, options.confidenceLow,
                                                   options.confidenceHigh)) ||
                        !cv::imwrite((options.output / (prefix + "page-" + number + "-words.png")).string(),
                                     createOverlay(rendered, result, Engine::Ocr::Level::Word, options.confidenceLow,
                                                   options.confidenceHigh)))
                        throw std::runtime_error("output: OpenCV could not write debug PNG");

                    if (recognition.rotation == 180) {
                        cv::Mat upright;
                        cv::rotate(rendered, upright, cv::ROTATE_180);
                        if (!cv::imwrite(
                                (options.output / (prefix + "page-" + number + "-oriented-render.png")).string(),
                                upright) ||
                            !cv::imwrite(
                                (options.output / (prefix + "page-" + number + "-oriented-words.png")).string(),
                                createOverlay(upright, recognition.interpreted, Engine::Ocr::Level::Word,
                                              options.confidenceLow, options.confidenceHigh)))
                            throw std::runtime_error("output: could not write oriented debug PNG");
                    }
                }
                auto receipt = Engine::Receipt::Interpreter::interpret(recognition.interpreted, pageId);
                pages.push_back({index + 1, options.debug ? std::optional<std::string>(renderName) : std::nullopt,
                                 "complete", std::move(result), inputPath, identity, std::move(receipt),
                                 std::move(recognition.interpreted), recognition.rotation, recognition.rotationReason});
            } catch (const std::exception& error) {
                failed = true;
                pages.push_back(
                    {index + 1, std::nullopt, "failed: " + std::string(error.what()), {}, inputPath, identity});
            }
        }
    }
    Report::write(options.output, inputs.front(), pages, options.dpi, options.psm, options.confidenceLow,
                  options.confidenceHigh, engine.metadata());
    if (failed) {
        std::string details;
        for (const auto& page : pages) {
            if (page.status != "complete")
                details += " page " + std::to_string(page.page) + ": " + page.status;
        }
        throw std::runtime_error("batch:" + details);
    }

    // Financial mismatches do not masquerade as processing errors: exit 2 requests review.
    const bool mismatch = std::ranges::any_of(pages, [](const auto& page) {
        if (!page.receipt)
            return false;

        const auto& receipt = *page.receipt;
        return receipt.totalCheck.status == "mismatch" || receipt.taxTotalCheck.status == "mismatch" ||
               receipt.calculatedTotalCheck.status == "mismatch" ||
               std::ranges::any_of(receipt.taxes,
                                   [](const auto& tax) {
                                       return tax.grossCheck.status == "mismatch" ||
                                              tax.rateCheck.status == "mismatch" ||
                                              tax.itemsGrossCheck.status == "mismatch";
                                   }) ||
               std::ranges::any_of(receipt.items,
                                   [](const auto& item) { return item.amountCheck.status == "mismatch"; });
    });

    return mismatch ? 2 : 0;
}

std::string Application::documentId(const std::filesystem::path& source) {
    const auto path = std::filesystem::weakly_canonical(std::filesystem::absolute(source)).generic_string();
    constexpr char digits[] = "0123456789abcdef";
    std::string id = "doc-";
    for (const unsigned char character : path) {
        id += digits[character >> 4];
        id += digits[character & 15];
    }

    return id;
}

std::string Application::scoreClass(float confidence, float low, float high) {
    if (confidence >= high)
        return "high";

    if (confidence >= low)
        return "medium";

    return "low";
}

std::string Application::regionLabel(const ReceiptLedger::Engine::Ocr::Result& result, std::size_t regionIndex) {
    const auto level = result.regions.at(regionIndex).level;
    const char prefix = level == ReceiptLedger::Engine::Ocr::Level::Line   ? 'L'
                        : level == ReceiptLedger::Engine::Ocr::Level::Word ? 'W'
                                                                           : 'S';
    std::size_t number = 0;
    for (std::size_t index = 0; index <= regionIndex; ++index)
        number += result.regions[index].level == level;

    return prefix + std::to_string(number);
}

void Application::validate(const Options& options) {
    if (options.inputs.empty() && !std::filesystem::is_regular_file(options.input))
        throw std::invalid_argument("input: PDF does not exist: " + options.input.string());

    if (options.dpi < 72 || options.dpi > 1200)
        throw std::invalid_argument("arguments: dpi must be between 72 and 1200");

    if (options.psm < 0 || options.psm > 13)
        throw std::invalid_argument("arguments: psm must be between 0 and 13");

    if (!std::isfinite(options.confidenceLow) || !std::isfinite(options.confidenceHigh) ||
        options.confidenceLow < 0.0F || options.confidenceHigh > 100.0F ||
        options.confidenceLow >= options.confidenceHigh)
        throw std::invalid_argument("arguments: confidence thresholds require 0 <= low < high <= 100");
}

} // namespace ReceiptLedger::App::PDF2CSV
