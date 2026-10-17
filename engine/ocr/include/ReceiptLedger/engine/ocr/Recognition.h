#pragma once

#include <ReceiptLedger/engine/ocr/Result.h>

namespace ReceiptLedger::Engine::Ocr {

struct Recognition {
    Result source;
    Result interpreted;
    int rotation = 0;
    std::string rotationReason;
};

} // namespace ReceiptLedger::Engine::Ocr
