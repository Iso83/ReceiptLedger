#include "Report.h"

#include "Application.h"
#include "LedgerReport.h"

#include <ReceiptLedger/Version.h>
#include <fstream>

namespace ReceiptLedger::App::PDF2CSV {

namespace OCR = ReceiptLedger::Engine::Ocr;

std::string Report::levelName(OCR::Level level) {
    using OCR::Level;
    if (level == Level::Line)
        return "line";

    if (level == Level::Word)
        return "word";

    return "symbol";
}

std::string Report::csvQuote(std::string text) {
    if (!text.empty() && (text.front() == '=' || text.front() == '+' || text.front() == '-' || text.front() == '@'))
        text.insert(text.begin(), '\'');

    std::string escaped;
    for (const char character : text) {
        escaped += character;
        if (character == '"')
            escaped += '"';
    }

    return '"' + escaped + '"';
}

std::string Report::markdown(std::string text) {
    std::string result;
    for (const char character : text) {
        if (character == '|')
            result += "\\|";
        else if (character == '\n' || character == '\r')
            result += "<br>";
        else
            result += character;
    }

    return result;
}

void Report::verifyWrite(std::ofstream& stream, const std::filesystem::path& path) {
    stream.flush();
    if (!stream)
        throw std::runtime_error("output: could not finish report file " + path.string());

    stream.close();
    if (!stream)
        throw std::runtime_error("output: could not close report file " + path.string());
}

void Report::write(const std::filesystem::path& directory, const std::filesystem::path& source,
                   const std::vector<PageResult>& pages, int dpi, int psm, float low, float high,
                   const OCR::Metadata& metadata) {
    const bool complete = std::ranges::all_of(pages, [](const PageResult& page) { return page.status == "complete"; });
    nlohmann::json root{{"source", source.filename().string()},
                        {"status", complete ? "complete" : "failed"},
                        {"application", "ReceiptLedger PDF2CSV"},
                        {"application_version", ReceiptLedger::Version},
                        {"engine", "Tesseract " + metadata.engineVersion},
                        {"language", metadata.language},
                        {"dpi", dpi},
                        {"psm", psm},
                        {"model_path", metadata.modelPath.string()},
                        {"model_sha256", metadata.modelSha256},
                        {"coordinates", "input image, top-left origin, pixels, [left,top,right,bottom)"},
                        {"preprocessing", "BGR/BGRA to 8-bit grayscale; no geometric transform"},
                        {"coverage", "Only reported OCR regions were investigated; page background was not assessed"},
                        {"thresholds", {{"low", low}, {"high", high}}},
                        {"pages", nlohmann::json::array()}};
    root["exit_codes"] = {{"0", "processing complete, no detected financial mismatch; unknown/review is not approval"},
                          {"1", "argument, input, processing or publication failure"},
                          {"2", "processing complete with financial mismatch"}};
    root["sources"] = nlohmann::json::array();
    for (const auto& page : pages) {
        const auto inputSource = page.source.empty() ? source : page.source;
        const auto name = inputSource.string();
        if (std::find(root["sources"].begin(), root["sources"].end(), nlohmann::json(name)) == root["sources"].end())
            root["sources"].push_back(name);
    }
    const auto csvPath = directory / "ocr.csv";
    const auto markdownPath = directory / "ocr-regions.md";
    const auto jsonPath = directory / "ocr.json";
    const auto csvTemporary = directory / ".ocr.csv.tmp";
    const auto markdownTemporary = directory / ".ocr-regions.md.tmp";
    const auto jsonTemporary = directory / ".ocr.json.tmp";
    std::error_code removeError;
    for (const auto* filename : {"ocr.json", "ocr.csv", "ocr-regions.md", "receipts.csv", "items.csv"}) {
        std::filesystem::remove(directory / filename, removeError);
        if (removeError)
            throw std::runtime_error("output: could not invalidate previous report " + (directory / filename).string());
    }

    if (removeError)
        throw std::runtime_error("output: could not invalidate previous success report " + jsonPath.string());

    std::ofstream csv(csvTemporary, std::ios::binary | std::ios::trunc);
    std::ofstream md(markdownTemporary, std::ios::binary | std::ios::trunc);
    std::ofstream json(jsonTemporary, std::ios::binary | std::ios::trunc);
    if (!csv || !md || !json)
        throw std::runtime_error("output: could not create report files in " + directory.string());

    csv << "\xEF\xBB\xBF"
           "document-id;source;page;region-id;label;level;parent-id;reading-order;text;left;top;right;bottom;"
           "confidence;"
           "score-class\n";

    md << "# OCR regions\n\nRun status: `" << (complete ? "complete" : "failed") << "`. Engine: Tesseract "
       << metadata.engineVersion << "; model: `" << markdown(metadata.modelPath.string()) << "`; model SHA-256: `"
       << metadata.modelSha256 << "`; application: `" << ReceiptLedger::Version << "` (`"
       << "); language: `" << metadata.language << "`; DPI: " << dpi << "; PSM: " << psm
       << ".  \nCoordinates: input image, top-left pixels, `[left, top, right, bottom)`.  \n"
       << "Preprocessing: grayscale only (identity geometry). Only reported OCR regions were investigated; page "
          "background was not assessed. Empty or rejected engine regions are not reported. Score legend: green >= "
       << high << ", orange >= " << low << " and < " << high << ", red < " << low
       << ", gray = unavailable. Match status is always `not_checked`.\n";
    for (const auto& page : pages) {
        const auto pageSource = page.source.empty() ? source : page.source;
        const auto identity = page.documentId.empty() ? Application::documentId(pageSource) : page.documentId;
        nlohmann::json pageJson{
            {"source", pageSource.string()},
            {"document_id", identity},
            {"page", page.page},
            {"status", page.status},
            {"width", page.ocr.width},
            {"height", page.ocr.height},
            {"reference_image", page.referenceImage ? nlohmann::json(*page.referenceImage) : nlohmann::json(nullptr)},
            {"text", page.ocr.text},
            {"no_text", page.ocr.regions.empty()},
            {"regions", nlohmann::json::array()}};
        if (page.interpretationOcr && page.referenceImage && page.rotation == 180) {
            auto image = *page.referenceImage;
            const auto suffix = image.rfind("-render.png");
            if (suffix != std::string::npos)
                image.replace(suffix, std::string::npos, "-oriented-render.png");

            pageJson["interpretation_reference_image"] = image;
        }
        if (page.interpretationOcr)
            pageJson["interpretation_ocr"] =
                LedgerReport::ocr(*page.interpretationOcr, page.rotation, page.rotationReason);

        if (page.receipt)
            pageJson["receipt"] = LedgerReport::json(*page.receipt);

        md << "\n## Page " << page.page << "\n\nStatus: `" << page.status << "`. Reference: ";
        if (page.referenceImage)
            md << '`' << *page.referenceImage << '`';
        else
            md << "not generated";
        md << " (" << page.ocr.width << "x" << page.ocr.height << "). Text status: `"
           << (page.ocr.regions.empty() ? "no_text" : "recognized")
           << "`.\n\n| ID | Label | Level | Parent | Order | Text | Box | Confidence | Class | Match | Notes |\n"
           << "|---|---|---|---|---:|---|---|---:|---|---|---|\n";
        int highCount = 0;
        int mediumCount = 0;
        int lowCount = 0;
        int unavailableCount = 0;
        for (std::size_t index = 0; index < page.ocr.regions.size(); ++index) {
            const auto& region = page.ocr.regions[index];
            const std::string classification =
                region.confidence ? Application::scoreClass(*region.confidence, low, high) : "unavailable";
            highCount += classification == "high";
            mediumCount += classification == "medium";
            lowCount += classification == "low";
            unavailableCount += classification == "unavailable";
            const std::string label = Application::regionLabel(page.ocr, index);

            nlohmann::json regionJson{
                {"id", region.id},
                {"label", label},
                {"level", levelName(region.level)},
                {"parent_id", region.parentId.empty() ? nlohmann::json(nullptr) : nlohmann::json(region.parentId)},
                {"reading_order", region.readingOrder},
                {"text", region.text},
                {"box", {region.left, region.top, region.right, region.bottom}},
                {"confidence", region.confidence ? nlohmann::json(*region.confidence) : nlohmann::json(nullptr)},
                {"score_class", classification},
                {"match_status", "not_checked"},
                {"notes", ""}};
            pageJson["regions"].push_back(regionJson);
            std::ostringstream confidence;
            if (region.confidence)
                confidence << std::fixed << std::setprecision(2) << *region.confidence;

            std::string localized = confidence.str();
            if (const auto point = localized.find('.'); point != std::string::npos)
                localized[point] = ',';

            csv << csvQuote(identity) << ';' << csvQuote(pageSource.filename().string()) << ';' << page.page << ';'
                << csvQuote(region.id) << ';' << csvQuote(label) << ';' << levelName(region.level) << ';'
                << csvQuote(region.parentId) << ';' << region.readingOrder << ';' << csvQuote(region.text) << ';'
                << region.left << ';' << region.top << ';' << region.right << ';' << region.bottom << ';' << localized
                << ';' << classification << '\n';
            md << '|' << region.id << '|' << label << '|' << levelName(region.level) << '|' << region.parentId << '|'
               << region.readingOrder << '|' << markdown(region.text) << "|[" << region.left << ", " << region.top
               << ", " << region.right << ", " << region.bottom << ")|" << confidence.str() << '|' << classification
               << "|not_checked||\n";
        }
        md << "\nSummary: " << highCount << " high, " << mediumCount << " medium, " << lowCount << " low, "
           << unavailableCount << " unavailable.\n";
        root["pages"].push_back(std::move(pageJson));
    }
    LedgerReport::writeTemporary(directory, pages);
    json << root.dump(2) << '\n';
    verifyWrite(csv, csvTemporary);
    verifyWrite(md, markdownTemporary);
    verifyWrite(json, jsonTemporary);

    const auto publish = [](const std::filesystem::path& temporary, const std::filesystem::path& destination) {
        std::error_code error;
        std::filesystem::remove(destination, error);
        if (!error)
            std::filesystem::rename(temporary, destination, error);
        if (error)
            throw std::runtime_error("output: could not publish report file " + destination.string() + ": " +
                                     error.message());
    };
    publish(csvTemporary, csvPath);
    publish(markdownTemporary, markdownPath);
    publish(directory / ".items.csv.tmp", directory / "items.csv");
    publish(directory / ".receipts.csv.tmp", directory / "receipts.csv");
    publish(jsonTemporary, jsonPath); // The success/failure manifest is the commit marker.
}

} // namespace ReceiptLedger::App::PDF2CSV
