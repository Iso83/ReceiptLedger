#pragma once

#include <ReceiptLedger/engine/ocr/Engine.h>

namespace ReceiptLedger::App::PDF2CSV {

struct Options {
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path tessdata;
    std::string language = "nld";
    int dpi = 300;
    int psm = 3;
    float confidenceHigh = 85.0F;
    float confidenceLow = 60.0F;
    bool debug = false;
    std::vector<std::filesystem::path> inputs;
};

class Application {
private:
    static void prepareOutput(const std::filesystem::path& output);
    static std::vector<std::filesystem::path> expandInputs(const Options& options);
    static cv::Mat render(void* document, int index, int dpi);
    static cv::Mat inputImage(const cv::Mat& renderImage);

public:
    static cv::Mat createOverlay(const cv::Mat& source, const Engine::Ocr::Result& result, Engine::Ocr::Level level,
                                 float low, float high);
    static int run(const Options& options);
    static std::string documentId(const std::filesystem::path& source);
    static std::string scoreClass(float confidence, float low, float high);
    static std::string regionLabel(const Engine::Ocr::Result& result, std::size_t regionIndex);
    static void validate(const Options& options);
};

} // namespace ReceiptLedger::App::PDF2CSV
