#include "SHA256.h"

#include <ReceiptLedger/engine/ocr/Engine.h>
#include <opencv2/core/mat.hpp>
#include <tesseract/baseapi.h>

namespace ReceiptLedger::Engine::Ocr {

using namespace Internal;

Engine::Engine(const std::filesystem::path& tessdata, const std::string& language, int dpi, int pageSegmentationMode)
    : m_api(std::make_unique<tesseract::TessBaseAPI>()), m_dpi(dpi) {
    if (!std::filesystem::is_directory(tessdata))
        throw std::runtime_error("model: tessdata directory does not exist: " + tessdata.string());

    const auto modelPath = tessdata / (language + ".traineddata");
    if (!std::filesystem::is_regular_file(modelPath))
        throw std::runtime_error("model: language data does not exist: " + modelPath.string());

    m_metadata = {tesseract::TessBaseAPI::Version(), language, modelPath, Sha256::file(modelPath)};
    if (m_api->Init(tessdata.string().c_str(), language.c_str(), tesseract::OEM_LSTM_ONLY) != 0)
        throw std::runtime_error("model: Tesseract could not load language '" + language + "' from " +
                                 tessdata.string());

    m_api->SetPageSegMode(static_cast<tesseract::PageSegMode>(pageSegmentationMode));
}

Engine::~Engine() = default;

Result Engine::recognize(const cv::Mat& image, const std::string& pageId) {
    Result result;
    result.width = image.cols;
    result.height = image.rows;
    m_api->Clear();
    if (image.empty())
        return result;

    if (image.depth() != CV_8U || image.channels() != 1)
        throw std::invalid_argument("recognition: expected an 8-bit single-channel image for " + pageId);

    const auto first = image.ptr<std::uint8_t>()[0];
    bool uniform = true;
    for (int row = 0; row < image.rows && uniform; ++row)
        uniform = std::all_of(image.ptr<std::uint8_t>(row), image.ptr<std::uint8_t>(row) + image.cols,
                              [first](std::uint8_t value) { return value == first; });

    if (uniform)
        return result;

    m_api->SetImage(image.data, image.cols, image.rows, image.channels(), static_cast<int>(image.step));
    m_api->SetSourceResolution(m_dpi);
    if (m_api->Recognize(nullptr) != 0)
        throw std::runtime_error("recognition: Tesseract failed for " + pageId);

    std::unique_ptr<char[]> pageText(m_api->GetUTF8Text());
    if (pageText)
        result.text = pageText.get();

    if (std::ranges::all_of(result.text, [](unsigned char character) { return std::isspace(character); }))
        result.text.clear();

    std::unique_ptr<tesseract::ResultIterator> iterator(m_api->GetIterator());
    if (!iterator)
        return result;

    int lineNumber = 0;
    int wordNumber = 0;
    int symbolNumber = 0;
    std::string lineId;
    std::string wordId;
    do {
        if (iterator->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {
            ++lineNumber;
            wordNumber = 0;
            lineId = pageId + "-line-" + std::to_string(lineNumber);
            Region line{lineId, Level::Line};
            const bool hasBox =
                iterator->BoundingBox(tesseract::RIL_TEXTLINE, &line.left, &line.top, &line.right, &line.bottom);
            std::unique_ptr<char[]> text(iterator->GetUTF8Text(tesseract::RIL_TEXTLINE));
            if (text)
                line.text = text.get();

            const float confidence = iterator->Confidence(tesseract::RIL_TEXTLINE);
            if (confidence >= 0.0F)
                line.confidence = confidence;

            if (hasBox && hasText(line.text)) {
                line.readingOrder = static_cast<int>(result.regions.size());
                result.regions.push_back(std::move(line));
            } else
                lineId.clear();
        }
        if (iterator->IsAtBeginningOf(tesseract::RIL_WORD)) {
            ++wordNumber;
            symbolNumber = 0;
            wordId = lineId + "-word-" + std::to_string(wordNumber);
            Region word{wordId, Level::Word, lineId};
            const bool hasBox =
                iterator->BoundingBox(tesseract::RIL_WORD, &word.left, &word.top, &word.right, &word.bottom);
            std::unique_ptr<char[]> text(iterator->GetUTF8Text(tesseract::RIL_WORD));
            if (text)
                word.text = text.get();

            const float confidence = iterator->Confidence(tesseract::RIL_WORD);
            if (confidence >= 0.0F)
                word.confidence = confidence;

            if (hasBox && hasText(word.text) && !lineId.empty()) {
                word.readingOrder = static_cast<int>(result.regions.size());
                result.regions.push_back(std::move(word));
            } else
                wordId.clear();
        }

        ++symbolNumber;
        Region symbol{wordId + "-symbol-" + std::to_string(symbolNumber), Level::Symbol, wordId};
        const bool hasBox =
            iterator->BoundingBox(tesseract::RIL_SYMBOL, &symbol.left, &symbol.top, &symbol.right, &symbol.bottom);
        std::unique_ptr<char[]> text(iterator->GetUTF8Text(tesseract::RIL_SYMBOL));
        if (text)
            symbol.text = text.get();

        const float confidence = iterator->Confidence(tesseract::RIL_SYMBOL);
        if (confidence >= 0.0F)
            symbol.confidence = confidence;

        if (hasBox && hasText(symbol.text) && !wordId.empty()) {
            symbol.readingOrder = static_cast<int>(result.regions.size());
            result.regions.push_back(std::move(symbol));
        }
    } while (iterator->Next(tesseract::RIL_SYMBOL));

    return result;
}

} // namespace ReceiptLedger::Engine::Ocr
