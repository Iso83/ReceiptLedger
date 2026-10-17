#include "Application.h"
#include "Report.h"
#include "TestAssert.h"

#include <ReceiptLedger/Version.h>
#include <ReceiptLedger/engine/receipt/Interpreter.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>
#include <unordered_set>

using namespace ReceiptLedger::App::PDF2CSV;
using namespace ReceiptLedger::Engine::Ocr;

static std::filesystem::path temporaryDirectory(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("receiptledger-" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

static void writePdf(const std::filesystem::path& path, const std::vector<std::string>& pageTexts) {
    std::vector<std::string> objects;
    std::ostringstream kids;
    for (std::size_t page = 0; page < pageTexts.size(); ++page)
        kids << 4 + page * 2 << " 0 R ";

    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [" + kids.str() + "] /Count " + std::to_string(pageTexts.size()) + " >>");
    objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>");
    for (std::size_t page = 0; page < pageTexts.size(); ++page) {
        const int content = 5 + static_cast<int>(page) * 2;
        objects.push_back(
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents " +
            std::to_string(content) + " 0 R >>");
        const std::string commands = "BT /F1 36 Tf 72 650 Td (" + pageTexts[page] + ") Tj ET\n";
        objects.push_back("<< /Length " + std::to_string(commands.size()) + " >>\nstream\n" + commands + "endstream");
    }

    std::ofstream output(path, std::ios::binary);
    output << "%PDF-1.4\n";
    std::vector<std::streamoff> offsets{0};
    for (std::size_t index = 0; index < objects.size(); ++index) {
        offsets.push_back(output.tellp());
        output << index + 1 << " 0 obj\n" << objects[index] << "\nendobj\n";
    }
    const auto xref = output.tellp();
    output << "xref\n0 " << objects.size() + 1 << "\n0000000000 65535 f \n";
    for (std::size_t index = 1; index < offsets.size(); ++index)
        output << std::setw(10) << std::setfill('0') << offsets[index] << " 00000 n \n";

    output << "trailer\n<< /Size " << objects.size() + 1 << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";
}

static std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

static std::vector<std::vector<std::string>> readCsv(const std::filesystem::path& path) {
    std::string contents = read(path);
    if (!contents.starts_with("\xEF\xBB\xBF"))
        throw std::runtime_error("CSV is missing its UTF-8 BOM");

    contents.erase(0, 3);
    std::vector<std::vector<std::string>> rows(1);
    std::string field;
    bool quoted = false;
    for (std::size_t index = 0; index < contents.size(); ++index) {
        const char character = contents[index];
        if (character == '"') {
            if (quoted && index + 1 < contents.size() && contents[index + 1] == '"') {
                field += '"';
                ++index;
            } else
                quoted = !quoted;
        } else if (character == ';' && !quoted) {
            rows.back().push_back(field);
            field.clear();
        } else if (character == '\n' && !quoted) {
            rows.back().push_back(field);
            field.clear();
            rows.emplace_back();
        } else if (character != '\r' || quoted)
            field += character;
    }
    if (rows.back().empty())
        rows.pop_back();

    return rows;
}

static bool hasExactExportIds(const std::unordered_set<std::string>& expectedIds,
                              const std::vector<std::vector<std::string>>& csvRows, const std::string& markdown) {
    std::unordered_set<std::string> csvIds;
    for (auto row = csvRows.begin() + 1; row != csvRows.end(); ++row) {
        if (row->size() <= 3 || !csvIds.insert((*row)[3]).second)
            return false;
    }

    std::unordered_set<std::string> markdownIds;
    std::istringstream lines(markdown);
    for (std::string line; std::getline(lines, line);) {
        if (!line.starts_with('|') || line.starts_with("|---") || line.starts_with("| ID |"))
            continue;

        const auto separator = line.find('|', 1);
        if (separator == std::string::npos || !markdownIds.insert(line.substr(1, separator - 1)).second)
            return false;
    }

    return csvIds == expectedIds && markdownIds == expectedIds;
}

int test_pipeline_and_exports() {
    const auto directory = temporaryDirectory("pipeline");
    const auto pdf = directory / "fixture.pdf";
    const auto output = directory / "output";
    writePdf(pdf, {"ALPHA 123", "BETA 456", ""});
    Options options{pdf, output, RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 200, 7, 85.0F, 60.0F, true};
    CPPTEST_ASSERT(Application::run(options) == 0);

    std::ifstream jsonInput(output / "ocr.json");
    nlohmann::json report;
    jsonInput >> report;
    CPPTEST_ASSERT(report["status"] == "complete");
    CPPTEST_ASSERT(report["application"] == "ReceiptLedger PDF2CSV");
    CPPTEST_ASSERT(report["application_version"].get<std::string>() == ReceiptLedger::Version);
    CPPTEST_ASSERT(report["coverage"].get<std::string>().find("background was not assessed") != std::string::npos);
    CPPTEST_ASSERT(report["pages"].size() == 3);
    CPPTEST_ASSERT(report["pages"][0]["text"].get<std::string>().find("ALPHA") != std::string::npos);
    CPPTEST_ASSERT(report["pages"][1]["text"].get<std::string>().find("BETA") != std::string::npos);
    CPPTEST_ASSERT(report["pages"][0]["text"].get<std::string>().find("BETA") == std::string::npos);
    CPPTEST_ASSERT(report["pages"][1]["text"].get<std::string>().find("ALPHA") == std::string::npos);
    CPPTEST_ASSERT(report["pages"][2]["no_text"] == true);
    CPPTEST_ASSERT(report["pages"][2]["regions"].empty());

    std::unordered_map<std::string, nlohmann::json> jsonRegions;
    for (const auto& page : report["pages"]) {
        std::unordered_set<std::string> pageIds;
        std::unordered_map<std::string, std::string> pageLevels;
        for (const auto& region : page["regions"])
            pageLevels.emplace(region["id"].get<std::string>(), region["level"].get<std::string>());

        int previousOrder = -1;
        for (const auto& region : page["regions"]) {
            const auto id = region["id"].get<std::string>();
            CPPTEST_ASSERT(pageIds.insert(id).second);
            CPPTEST_ASSERT(!jsonRegions.contains(id));
            CPPTEST_ASSERT(region["reading_order"].get<int>() > previousOrder);
            previousOrder = region["reading_order"].get<int>();

            const auto box = region["box"];
            CPPTEST_ASSERT(box[0] >= 0 && box[1] >= 0 && box[2] > box[0] && box[3] > box[1]);
            CPPTEST_ASSERT(box[2] <= page["width"] && box[3] <= page["height"]);
            const auto level = region["level"].get<std::string>();
            if (level == "line")
                CPPTEST_ASSERT(region["parent_id"].is_null());
            else {
                CPPTEST_ASSERT(!region["parent_id"].is_null());
                const auto parent = region["parent_id"].get<std::string>();
                CPPTEST_ASSERT(pageIds.contains(parent));
                CPPTEST_ASSERT(pageLevels.at(parent) == (level == "word" ? "line" : "word"));
            }
            jsonRegions.emplace(id, region);
        }
    }

    for (int page = 1; page <= 3; ++page) {
        std::ostringstream pageNumber;
        pageNumber << std::setw(4) << std::setfill('0') << page;
        const std::string number = pageNumber.str();
        for (const auto* kind : {"render", "input", "symbols", "words"}) {
            const cv::Mat image =
                cv::imread((output / ("page-" + number + "-" + kind + ".png")).string(), cv::IMREAD_UNCHANGED);
            CPPTEST_ASSERT(!image.empty());
            if (std::string(kind) == "symbols" || std::string(kind) == "words")
                CPPTEST_ASSERT(image.cols > report["pages"][page - 1]["width"]);
            else
                CPPTEST_ASSERT(image.cols == report["pages"][page - 1]["width"]);
            CPPTEST_ASSERT(image.rows == report["pages"][page - 1]["height"]);
        }

        const cv::Mat render = cv::imread((output / ("page-" + number + "-render.png")).string());
        const cv::Mat words = cv::imread((output / ("page-" + number + "-words.png")).string());
        const cv::Mat symbols = cv::imread((output / ("page-" + number + "-symbols.png")).string());
        CPPTEST_ASSERT(words.at<cv::Vec3b>(render.rows - 1, render.cols - 1) ==
                       render.at<cv::Vec3b>(render.rows - 1, render.cols - 1));
        for (const auto& region : report["pages"][page - 1]["regions"]) {
            const auto classification = region["score_class"].get<std::string>();
            const cv::Vec3b expected = classification == "high"     ? cv::Vec3b(0, 170, 0)
                                       : classification == "medium" ? cv::Vec3b(0, 165, 255)
                                       : classification == "low"    ? cv::Vec3b(0, 0, 220)
                                                                    : cv::Vec3b(128, 128, 128);
            const auto box = region["box"];
            if (region["level"] == "word")
                CPPTEST_ASSERT(words.at<cv::Vec3b>(box[1], box[0]) == expected);
            if (region["level"] == "symbol")
                CPPTEST_ASSERT(symbols.at<cv::Vec3b>(box[1], box[0]) == expected);
        }
    }

    const std::string csv = read(output / "ocr.csv");
    const std::string markdown = read(output / "ocr-regions.md");
    CPPTEST_ASSERT(csv.starts_with("\xEF\xBB\xBF"));
    for (const auto& page : report["pages"])
        for (const auto& region : page["regions"]) {
            CPPTEST_ASSERT(csv.find(region["id"].get<std::string>()) != std::string::npos);
            const std::string parent = region["parent_id"].is_null() ? "" : region["parent_id"].get<std::string>();
            const std::string markdownPrefix = "|" + region["id"].get<std::string>() + "|" +
                                               region["label"].get<std::string>() + "|" +
                                               region["level"].get<std::string>() + "|" + parent + "|" +
                                               std::to_string(region["reading_order"].get<int>()) + "|";
            CPPTEST_ASSERT(markdown.find(markdownPrefix) != std::string::npos);
        }

    const auto rows = readCsv(output / "ocr.csv");
    CPPTEST_ASSERT(rows.size() == jsonRegions.size() + 1);
    CPPTEST_ASSERT(rows.front().size() == 15);
    std::unordered_set<std::string> expectedIds;
    for (const auto& entry : jsonRegions)
        CPPTEST_ASSERT(expectedIds.insert(entry.first).second);

    CPPTEST_ASSERT(hasExactExportIds(expectedIds, rows, markdown));
    for (auto row = rows.begin() + 1; row != rows.end(); ++row) {
        CPPTEST_ASSERT(row->size() == 15);

        const auto region = jsonRegions.find((*row)[3]);
        CPPTEST_ASSERT(region != jsonRegions.end());
        CPPTEST_ASSERT((*row)[0] == Application::documentId(pdf));
        CPPTEST_ASSERT((*row)[1] == pdf.filename().string());
        CPPTEST_ASSERT(std::stoi((*row)[2]) == std::stoi(region->first.substr(region->first.find("page-") + 5, 4)));
        CPPTEST_ASSERT((*row)[4] == region->second["label"].get<std::string>());
        CPPTEST_ASSERT((*row)[5] == region->second["level"].get<std::string>());
        CPPTEST_ASSERT((*row)[6] ==
                       (region->second["parent_id"].is_null() ? "" : region->second["parent_id"].get<std::string>()));
        CPPTEST_ASSERT(std::stoi((*row)[7]) == region->second["reading_order"]);
        CPPTEST_ASSERT((*row)[8] == region->second["text"].get<std::string>());
        for (int field = 0; field < 4; ++field)
            CPPTEST_ASSERT(std::stoi((*row)[9 + field]) == region->second["box"][field]);

        if (region->second["confidence"].is_null())
            CPPTEST_ASSERT((*row)[13].empty());
        else {
            std::string confidence = (*row)[13];
            std::ranges::replace(confidence, ',', '.');
            CPPTEST_ASSERT(std::abs(std::stof(confidence) - region->second["confidence"].get<float>()) < 0.006F);
        }
        CPPTEST_ASSERT((*row)[14] == region->second["score_class"].get<std::string>());

        std::ostringstream confidence;
        if (!region->second["confidence"].is_null())
            confidence << std::fixed << std::setprecision(2) << region->second["confidence"].get<float>();
        std::string markdownText = region->second["text"];
        for (std::size_t position = 0; (position = markdownText.find('|', position)) != std::string::npos;
             position += 2)
            markdownText.replace(position, 1, "\\|");
        for (std::size_t position = 0; position < markdownText.size();) {
            if (markdownText[position] == '\r' || markdownText[position] == '\n') {
                markdownText.replace(position, 1, "<br>");
                position += 4;
            } else
                ++position;
        }
        const auto box = region->second["box"];
        std::ostringstream expectedMarkdown;
        expectedMarkdown << '|' << region->first << '|' << region->second["label"].get<std::string>() << '|'
                         << region->second["level"].get<std::string>() << '|' << (*row)[6] << '|' << (*row)[7] << '|'
                         << markdownText << "|[" << box[0] << ", " << box[1] << ", " << box[2] << ", " << box[3] << ")|"
                         << confidence.str() << '|' << (*row)[14] << "|not_checked||";
        CPPTEST_ASSERT(markdown.find(expectedMarkdown.str()) != std::string::npos);
        CPPTEST_ASSERT(markdown.find(expectedMarkdown.str(), markdown.find(expectedMarkdown.str()) + 1) ==
                       std::string::npos);
    }

    auto duplicateCsvRows = rows;
    duplicateCsvRows.back()[3] = duplicateCsvRows[1][3];
    CPPTEST_ASSERT(!hasExactExportIds(expectedIds, duplicateCsvRows, markdown));
    const std::string duplicateMarkdown = markdown + "|" + *expectedIds.begin() + "|extra|row|\n";
    CPPTEST_ASSERT(!hasExactExportIds(expectedIds, rows, duplicateMarkdown));
    const std::string extraMarkdown = markdown + "|unexpected-region|extra|row|\n";
    CPPTEST_ASSERT(!hasExactExportIds(expectedIds, rows, extraMarkdown));

    return 0;
}

int test_report_escaping_and_no_debug_reference() {
    const auto directory = temporaryDirectory("report");

    Result result;
    result.text = "=sum; \"café\"\r\nnext|row\rlast";
    result.width = 100;
    result.height = 80;
    result.regions.push_back({"page-line-1", Level::Line, "", 0, result.text, 1, 2, 90, 70, 72.345F});
    result.regions.push_back({"page-line-2", Level::Line, "", 1, "plain", 3, 4, 80, 60, std::nullopt});

    Metadata metadata{"5.test", "nld", directory / "nld.traineddata", "hash"};
    Report::write(directory, "formula.pdf", {{1, std::nullopt, "complete", result}}, 300, 3, 60.0F, 85.0F, metadata);
    const auto json = nlohmann::json::parse(read(directory / "ocr.json"));
    CPPTEST_ASSERT(json["pages"][0]["reference_image"].is_null());
    CPPTEST_ASSERT(json["pages"][0]["regions"][0]["text"] == result.text);
    const std::string csv = read(directory / "ocr.csv");
    CPPTEST_ASSERT(csv.find("\"'=sum; \"\"café\"\"\r\nnext|row\rlast\"") != std::string::npos);

    const auto rows = readCsv(directory / "ocr.csv");
    CPPTEST_ASSERT(rows.size() == 3);
    CPPTEST_ASSERT(rows[1][8] == "'=sum; \"café\"\r\nnext|row\rlast");
    CPPTEST_ASSERT(rows[1][13] == "72,35");
    CPPTEST_ASSERT(rows[2][13].empty());
    const std::string markdown = read(directory / "ocr-regions.md");
    CPPTEST_ASSERT(markdown.find("=sum; \"café\"<br><br>next\\|row<br>last") != std::string::npos);

    return 0;
}

int test_thresholds_overlay_and_validation() {
    CPPTEST_ASSERT(Application::scoreClass(59.99F, 60.0F, 85.0F) == "low");
    CPPTEST_ASSERT(Application::scoreClass(60.0F, 60.0F, 85.0F) == "medium");
    CPPTEST_ASSERT(Application::scoreClass(85.0F, 60.0F, 85.0F) == "high");
    CPPTEST_ASSERT(Application::scoreClass(100.0F, 60.0F, 85.0F) == "high");

    Result result;
    result.regions.push_back({"word-high", Level::Word, "line", 0, "high", 50, 60, 100, 90, 85.0F});
    result.regions.push_back({"word-medium", Level::Word, "line", 1, "medium", 110, 60, 160, 90, 60.0F});
    result.regions.push_back({"word-low", Level::Word, "line", 2, "low", 170, 60, 220, 90, 59.0F});
    result.regions.push_back({"word-none", Level::Word, "line", 3, "none", 230, 60, 280, 90, std::nullopt});

    cv::Mat source(120, 320, CV_8UC3, cv::Scalar(255, 255, 255));
    const cv::Mat overlay = Application::createOverlay(source, result, Level::Word, 60.0F, 85.0F);
    CPPTEST_ASSERT(overlay.cols > source.cols);
    CPPTEST_ASSERT(overlay.rows == source.rows);
    CPPTEST_ASSERT(overlay.at<cv::Vec3b>(60, 50) == cv::Vec3b(0, 170, 0));
    CPPTEST_ASSERT(overlay.at<cv::Vec3b>(60, 110) == cv::Vec3b(0, 165, 255));
    CPPTEST_ASSERT(overlay.at<cv::Vec3b>(60, 170) == cv::Vec3b(0, 0, 220));
    CPPTEST_ASSERT(overlay.at<cv::Vec3b>(60, 230) == cv::Vec3b(128, 128, 128));

    Result dense;
    for (int index = 0; index < 20; ++index)
        dense.regions.push_back({"symbol-" + std::to_string(index), Level::Symbol, "word", index, "X", 80 + index % 15,
                                 8 + index * 4, 96 + index % 15, 11 + index * 4, 90.0F});
    cv::Mat narrow(100, 100, CV_8UC3, cv::Scalar(240, 241, 242));
    const cv::Mat denseOverlay = Application::createOverlay(narrow, dense, Level::Symbol, 60.0F, 85.0F);
    CPPTEST_ASSERT(denseOverlay.cols > narrow.cols);
    CPPTEST_ASSERT(denseOverlay.at<cv::Vec3b>(99, 99) == narrow.at<cv::Vec3b>(99, 99));
    CPPTEST_ASSERT(denseOverlay.at<cv::Vec3b>(8, 80) == cv::Vec3b(0, 170, 0));

    const cv::Scalar panelMean =
        cv::mean(denseOverlay(cv::Rect(narrow.cols, 0, denseOverlay.cols - narrow.cols, denseOverlay.rows)));
    CPPTEST_ASSERT(panelMean[0] < 255.0 || panelMean[1] < 255.0 || panelMean[2] < 255.0);

    const auto directory = temporaryDirectory("validation");
    const auto pdf = directory / "valid.pdf";
    writePdf(pdf, {"VALID"});
    Options options{pdf, directory / "output", RECEIPTLEDGER_DEFAULT_TESSDATA_DIR};
    for (const float invalid : {std::nanf(""), std::numeric_limits<float>::infinity()}) {
        options.confidenceLow = invalid;
        try {
            Application::validate(options);
        } catch (const std::invalid_argument&) {
            continue;
        }
        CPPTEST_ASSERT(false);
    }
    for (const auto thresholds :
         {std::pair{85.0F, 85.0F}, std::pair{85.0F, 60.0F}, std::pair{-1.0F, 85.0F}, std::pair{60.0F, 101.0F}}) {
        options.confidenceLow = thresholds.first;
        options.confidenceHigh = thresholds.second;
        bool rejected = false;
        try {
            Application::validate(options);
        } catch (const std::invalid_argument& error) {
            rejected = std::string(error.what()).find("threshold") != std::string::npos;
        }
        CPPTEST_ASSERT(rejected);
    }
    options.confidenceLow = 60.0F;
    options.confidenceHigh = 85.0F;
    for (const int dpi : {71, 1201}) {
        options.dpi = dpi;
        bool rejected = false;
        try {
            Application::validate(options);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CPPTEST_ASSERT(rejected);
    }

    return 0;
}

int test_pipeline_failures() {
    const auto directory = temporaryDirectory("failures");
    const auto invalidPdf = directory / "invalid.pdf";
    std::ofstream(invalidPdf) << "not a pdf";
    Options options{invalidPdf, directory / "output", RECEIPTLEDGER_DEFAULT_TESSDATA_DIR};
    bool invalidPdfRejected = false;
    try {
        Application::run(options);
    } catch (const std::runtime_error& error) {
        CPPTEST_ASSERT(std::string(error.what()).find("PDFium") != std::string::npos);
        invalidPdfRejected = true;
    }
    CPPTEST_ASSERT(invalidPdfRejected);

    const auto validPdf = directory / "valid.pdf";
    writePdf(validPdf, {"VALID"});
    options.input = validPdf;
    options.tessdata = directory / "missing-model";
    bool missingModelRejected = false;
    try {
        Application::run(options);
    } catch (const std::runtime_error& error) {
        CPPTEST_ASSERT(std::string(error.what()).find("tessdata") != std::string::npos);
        missingModelRejected = true;
    }
    CPPTEST_ASSERT(missingModelRejected);

    options.tessdata = RECEIPTLEDGER_DEFAULT_TESSDATA_DIR;
    options.output = directory / "output-file";
    std::ofstream(options.output) << "occupied";
    try {
        Application::run(options);
    } catch (const std::runtime_error& error) {
        CPPTEST_ASSERT(std::string(error.what()).find("not a directory") != std::string::npos);
        return 0;
    }
    CPPTEST_ASSERT(false);

    return 1;
}

int test_report_failure_invalidates_success_manifest() {
    const auto directory = temporaryDirectory("report-failure");
    std::ofstream(directory / "ocr.json") << R"({"status":"complete"})";
    std::filesystem::create_directory(directory / ".ocr.csv.tmp");
    Result result;
    Metadata metadata{"5.test", "nld", directory / "nld.traineddata", "hash"};
    bool rejected = false;
    try {
        Report::write(directory, "fixture.pdf", {{1, std::nullopt, "complete", result}}, 300, 3, 60.0F, 85.0F,
                      metadata);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("report files") != std::string::npos;
    }
    CPPTEST_ASSERT(rejected);
    CPPTEST_ASSERT(!std::filesystem::exists(directory / "ocr.json"));

    return 0;
}

int test_rejected_rerun_invalidates_previous_reports() {
    const auto directory = temporaryDirectory("rerun");
    const auto pdf = directory / "valid.pdf";
    const auto output = directory / "output";
    writePdf(pdf, {"VALID"});
    Options options{pdf, output, RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 200, 7};
    CPPTEST_ASSERT(Application::run(options) == 0);
    CPPTEST_ASSERT(nlohmann::json::parse(read(output / "ocr.json"))["status"] == "complete");

    for (const auto invalid : {"missing-input", "invalid-options"}) {
        if (std::string(invalid) == "missing-input")
            options.input = directory / "missing.pdf";
        else {
            options.input = pdf;
            options.dpi = 71;
        }

        bool rejected = false;
        try {
            Application::run(options);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        CPPTEST_ASSERT(rejected);
        CPPTEST_ASSERT(!std::filesystem::exists(output / "ocr.json"));
        CPPTEST_ASSERT(!std::filesystem::exists(output / "ocr.csv"));
        CPPTEST_ASSERT(!std::filesystem::exists(output / "ocr-regions.md"));

        options.input = pdf;
        options.dpi = 200;
        CPPTEST_ASSERT(Application::run(options) == 0);
    }

    return 0;
}

int test_partial_pipeline_failure_is_reported() {
    const auto directory = temporaryDirectory("partial-failure");
    const auto pdf = directory / "fixture.pdf";
    const auto output = directory / "output";
    writePdf(pdf, {"FIRST PAGE", "SECOND PAGE"});
    std::filesystem::create_directories(output / "page-0002-render.png");
    Options options{pdf, output, RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 200, 7, 85.0F, 60.0F, true};
    bool rejected = false;
    try {
        Application::run(options);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("page 2") != std::string::npos;
    }
    CPPTEST_ASSERT(rejected);
    const auto report = nlohmann::json::parse(read(output / "ocr.json"));
    CPPTEST_ASSERT(report["status"] == "failed");
    CPPTEST_ASSERT(report["pages"].size() == 2);
    CPPTEST_ASSERT(report["pages"][0]["status"] == "complete");
    CPPTEST_ASSERT(report["pages"][1]["status"].get<std::string>().starts_with("failed:"));

    return 0;
}

int test_ledger_exports_and_identity() {
    const auto directory = temporaryDirectory("ledger-exports");
    Result source;
    const std::vector<std::string> lines{"Artikel Prijs Totaal", "3 café; \"test\" 1,250 3,75", "Unknown 2,00",
                                         "TOTAAL 5,75", "D 21,00% 4,75 1,00 5,75"};
    for (std::size_t index = 0; index < lines.size(); ++index)
        source.regions.push_back({"line-" + std::to_string(index), Level::Line, "", static_cast<int>(index),
                                  lines[index], 0, static_cast<int>(index * 30), 600, static_cast<int>(index * 30 + 20),
                                  99.0F});

    const auto path = directory / "same.pdf";
    auto receipt =
        ReceiptLedger::Engine::Receipt::Interpreter::interpret(source, Application::documentId(path) + "-page-1");
    CPPTEST_ASSERT(receipt.items.size() == 2);
    source.width = 600;
    source.height = 200;
    PageResult page{1,       std::nullopt, "complete", source,         path, Application::documentId(path),
                    receipt, source,       180,        "test rotation"};
    Metadata metadata{"5.test", "nld", directory / "nld.traineddata", "hash"};
    Report::write(directory, path, {page}, 300, 3, 60.0F, 85.0F, metadata);

    const auto items = readCsv(directory / "items.csv");
    const auto receipts = readCsv(directory / "receipts.csv");
    CPPTEST_ASSERT(items.size() == 3);
    CPPTEST_ASSERT(items[1][5] == "café; \"test\"");
    CPPTEST_ASSERT(items[1][6] == "3");
    CPPTEST_ASSERT(items[1][8] == "1,250");
    CPPTEST_ASSERT(items[1][9] == "3,75");
    CPPTEST_ASSERT(items[1][10] == "3,75");
    CPPTEST_ASSERT(items[1][14] == "match");
    CPPTEST_ASSERT(items[2][6].empty());
    CPPTEST_ASSERT(items[2][14] == "not_checkable");
    CPPTEST_ASSERT(receipts[1][8] == "5,75");
    CPPTEST_ASSERT(receipts[1][11] == "match");
    CPPTEST_ASSERT(receipts[1][18] == "4,75");
    CPPTEST_ASSERT(receipts[1][19] == "1,00");
    CPPTEST_ASSERT(receipts[1][20] == "5,75");
    CPPTEST_ASSERT(nlohmann::json::parse(receipts[1][21]).size() == 1);

    const auto report = nlohmann::json::parse(read(directory / "ocr.json"));
    CPPTEST_ASSERT(report["pages"][0]["interpretation_ocr"]["rotation_degrees"] == 180);
    CPPTEST_ASSERT(report["pages"][0]["interpretation_ocr"]["to_source_transform"] ==
                   nlohmann::json({-1, 0, 600, 0, -1, 200}));
    CPPTEST_ASSERT(report["pages"][0]["receipt"]["items"][0]["quantity"]["origin"] == "printed");
    CPPTEST_ASSERT(!report["pages"][0]["receipt"]["items"][0]["quantity"]["regions"].empty());
    CPPTEST_ASSERT(Application::documentId(directory / "a" / "same.pdf") !=
                   Application::documentId(directory / "b" / "same.pdf"));
    CPPTEST_ASSERT(Application::documentId(path) == Application::documentId(directory / "." / "same.pdf"));
    return 0;
}

int test_batch_and_failed_rerun() {
    const auto directory = temporaryDirectory("batch");
    std::filesystem::create_directories(directory / "a");
    std::filesystem::create_directories(directory / "b");
    const auto first = directory / "a" / "same.pdf";
    const auto second = directory / "b" / "same.pdf";
    writePdf(first, {"ALPHA 123"});
    writePdf(second, {"BETA 456"});

    Options options;
    options.inputs = {first, second};
    options.output = directory / "output";
    options.tessdata = RECEIPTLEDGER_DEFAULT_TESSDATA_DIR;
    options.dpi = 150;
    options.psm = 7;
    CPPTEST_ASSERT(Application::run(options) == 0);

    auto report = nlohmann::json::parse(read(options.output / "ocr.json"));
    CPPTEST_ASSERT(report["pages"].size() == 2);
    CPPTEST_ASSERT(report["pages"][0]["document_id"] != report["pages"][1]["document_id"]);
    CPPTEST_ASSERT(report["pages"][0]["receipt"]["id"] != report["pages"][1]["receipt"]["id"]);
    CPPTEST_ASSERT(readCsv(options.output / "receipts.csv").size() == 3);
    options.inputs[1] = directory / "missing.pdf";

    bool failed = false;
    try {
        Application::run(options);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    CPPTEST_ASSERT(failed);
    report = nlohmann::json::parse(read(options.output / "ocr.json"));
    CPPTEST_ASSERT(report["status"] == "failed");
    CPPTEST_ASSERT(report["pages"][1]["status"].get<std::string>().starts_with("failed:"));
    CPPTEST_ASSERT(readCsv(options.output / "receipts.csv").size() == 2);
    CPPTEST_ASSERT(readCsv(options.output / "items.csv").size() == 1);
    return 0;
}

int test_folder_and_literal_wildcard_inputs() {
    const auto directory = temporaryDirectory("folder with spaces");
    const auto input = directory / "tickets";
    std::filesystem::create_directories(input / "nested");
    writePdf(input / "a.PDF", {"ALPHA 123"});
    writePdf(input / "b.pdf", {"BETA 456"});
    writePdf(input / "nested" / "ignored.pdf", {"NESTED 789"});
    std::ofstream(input / "ignored.txt") << "not a PDF";

    Options options;
    options.output = directory / "output";
    options.tessdata = RECEIPTLEDGER_DEFAULT_TESSDATA_DIR;
    options.dpi = 150;
    options.psm = 7;
    for (const auto& source : {input, input / "*.pdf", input / "?.PDF"}) {
        options.inputs = {source};
        CPPTEST_ASSERT(Application::run(options) == 0);
        const auto report = nlohmann::json::parse(read(options.output / "ocr.json"));
        CPPTEST_ASSERT(report["pages"].size() == 2);
        CPPTEST_ASSERT(report["pages"][0]["source"].get<std::string>().find("a.PDF") != std::string::npos);
        CPPTEST_ASSERT(readCsv(options.output / "receipts.csv").size() == 3);
    }

    // Single-input API also accepts a folder; the original explicit-file tests remain intact.
    options.inputs.clear();
    options.input = input;
    CPPTEST_ASSERT(Application::run(options) == 0);
    for (const auto& source : {input / "missing*.pdf", input / "nested" / "empty"}) {
        std::filesystem::create_directories(input / "nested" / "empty");
        options.inputs = {source};
        bool failed = false;
        try {
            Application::run(options);
        } catch (const std::invalid_argument& error) {
            failed = std::string(error.what()).find("no PDF files found") != std::string::npos;
        }
        CPPTEST_ASSERT(failed);
        CPPTEST_ASSERT(!std::filesystem::exists(options.output / "ocr.json"));
        CPPTEST_ASSERT(!std::filesystem::exists(options.output / "receipts.csv"));
    }
    return 0;
}

int main() {
    CPPTEST_RUN(test_folder_and_literal_wildcard_inputs);
    CPPTEST_RUN(test_batch_and_failed_rerun);
    CPPTEST_RUN(test_ledger_exports_and_identity);
    CPPTEST_RUN(test_pipeline_and_exports);
    CPPTEST_RUN(test_report_escaping_and_no_debug_reference);
    CPPTEST_RUN(test_thresholds_overlay_and_validation);
    CPPTEST_RUN(test_pipeline_failures);
    CPPTEST_RUN(test_report_failure_invalidates_success_manifest);
    CPPTEST_RUN(test_rejected_rerun_invalidates_previous_reports);
    CPPTEST_RUN(test_partial_pipeline_failure_is_reported);
    return 0;
}
