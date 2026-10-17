#pragma once

#include <ReceiptLedger/engine/ocr/Engine.h>
#include <ReceiptLedger/engine/receipt/Result.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ReceiptLedger::App::PDF2CSV {

struct PageResult {
    int page = 0;
    std::optional<std::string> referenceImage;
    std::string status = "complete";
    Engine::Ocr::Result ocr;
    std::filesystem::path source;
    std::string documentId;
    std::optional<Engine::Receipt::Result> receipt;
    std::optional<Engine::Ocr::Result> interpretationOcr;
    int rotation = 0;
    std::string rotationReason;
};

class Report {
private:
    static std::string levelName(Engine::Ocr::Level level);
    static std::string csvQuote(std::string text);
    static std::string markdown(std::string text);
    static void verifyWrite(std::ofstream& stream, const std::filesystem::path& path);

public:
    static void write(const std::filesystem::path& directory, const std::filesystem::path& source,
                      const std::vector<PageResult>& pages, int dpi, int psm, float low, float high,
                      const Engine::Ocr::Metadata& metadata);
};

} // namespace ReceiptLedger::App::PDF2CSV
