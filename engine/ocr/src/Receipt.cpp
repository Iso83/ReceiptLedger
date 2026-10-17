#include "Columns.h"

#include <opencv2/imgproc.hpp>
#include <regex>
#include <tesseract/baseapi.h>

namespace ReceiptLedger::Engine::Ocr {

Recognition Engine::recognizeReceipt(const cv::Mat& image, const std::string& pageId) {
    Recognition recognition;
    recognition.source = recognize(image, pageId);
    recognition.interpreted = recognition.source;
    const auto quality = [](const Result& result) {
        double total = 0;
        std::size_t weight = 0;
        for (const auto& region : result.regions) {
            if (region.level != Level::Word || !region.confidence)
                continue;

            const auto length = std::min<std::size_t>(region.text.size(), 20);
            total += *region.confidence * length;
            weight += length;
        }

        return weight ? total / weight : 0.0;
    };

    cv::Mat input = image;

    // Compare image recognition only; neither expected ticket data nor financial sums influence orientation.
    if (!image.empty() && quality(recognition.source) < 80.0) {
        cv::Mat rotated;
        cv::rotate(image, rotated, cv::ROTATE_180);
        auto alternative = recognize(rotated, pageId + "-rotation-180");
        if (quality(alternative) >= 60.0 && quality(alternative) >= quality(recognition.source) + 15.0) {
            recognition.interpreted = std::move(alternative);
            recognition.rotation = 180;
            recognition.rotationReason =
                "180 degree image OCR has at least 15 points higher character-weighted word confidence";
            input = rotated;
        }
    }
    Internal::Ocr::Columns::read(*this, input, recognition.interpreted);

    return recognition;
}

Region Engine::readField(const cv::Mat& image, int left, int top, int right, int bottom, const std::string& parentId,
                         const std::string& purpose) {
    if (left < 0 || top < 0 || right > image.cols || bottom > image.rows || right <= left || bottom <= top)
        throw std::invalid_argument("recognition: invalid field rectangle");

    const auto previousMode = m_api->GetPageSegMode();
    m_api->SetPageSegMode(purpose == "description" ? tesseract::PSM_SINGLE_BLOCK : tesseract::PSM_SINGLE_LINE);
    Result reading;
    try {
        reading = recognize(image(cv::Rect(left, top, right - left, bottom - top)), parentId + "-field-" + purpose);
    } catch (...) {
        m_api->SetPageSegMode(previousMode);
        throw;
    }

    m_api->SetPageSegMode(previousMode);
    Region field{parentId + "-field-" + purpose, Level::Line, parentId, 0, reading.text, left, top, right, bottom};
    std::string descriptionLine;
    if (purpose == "description") {
        for (const auto& region : reading.regions) {
            if (region.level == Level::Line &&
                std::regex_search(region.text, std::regex(R"(^[^a-zA-Z0-9+-]*[0-9]+\s+[^\r\n]+)")))
                descriptionLine = region.id;
        }
    }

    double confidence = 0;
    int count = 0;
    for (const auto& region : reading.regions) {
        if (region.level == Level::Word && region.confidence &&
            (descriptionLine.empty() || region.parentId == descriptionLine)) {
            confidence += *region.confidence;
            ++count;
        }
    }
    if (count)
        field.confidence = static_cast<float>(confidence / count);

    field.purpose = purpose;
    return field;
}

} // namespace ReceiptLedger::Engine::Ocr
