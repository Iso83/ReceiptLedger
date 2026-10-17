#pragma once

#include <ReceiptLedger/engine/ocr/Engine.h>

namespace ReceiptLedger::Internal::Ocr {

class Columns {
public:
    static void read(Engine::Ocr::Engine& engine, const cv::Mat& image, Engine::Ocr::Result& result);
};

} // namespace ReceiptLedger::Internal::Ocr
