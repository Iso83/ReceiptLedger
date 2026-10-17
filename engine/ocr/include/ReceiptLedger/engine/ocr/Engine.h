#pragma once

#include <ReceiptLedger/engine/ocr/Recognition.h>
#include <filesystem>

namespace cv {
class Mat;
}

namespace tesseract {
class TessBaseAPI;
}

namespace ReceiptLedger::Engine::Ocr {

struct Metadata {
    std::string engineVersion;
    std::string language;
    std::filesystem::path modelPath;
    std::string modelSha256;
};

class Engine {
private:
    std::unique_ptr<tesseract::TessBaseAPI> m_api;
    int m_dpi;
    Metadata m_metadata;

public:
    Engine(const std::filesystem::path& tessdata, const std::string& language, int dpi, int pageSegmentationMode);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Result recognize(const cv::Mat& image, const std::string& pageId);
    Recognition recognizeReceipt(const cv::Mat& image, const std::string& pageId);
    Region readField(const cv::Mat& image, int left, int top, int right, int bottom, const std::string& parentId,
                     const std::string& purpose);
    const Metadata& metadata() const {
        return m_metadata;
    }

private:
    static bool hasText(const std::string& text) {
        return std::ranges::any_of(text, [](unsigned char character) { return !std::isspace(character); });
    }
};

} // namespace ReceiptLedger::Engine::Ocr
