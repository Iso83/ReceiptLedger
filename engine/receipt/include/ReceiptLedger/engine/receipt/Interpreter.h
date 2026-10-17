#pragma once

#include <ReceiptLedger/engine/ocr/Result.h>
#include <ReceiptLedger/engine/receipt/Result.h>

namespace ReceiptLedger::Engine::Receipt {

class Interpreter {
public:
    static Result interpret(const Ocr::Result& source, const std::string& pageId);
    static void reconcile(Result& receipt);
};

} // namespace ReceiptLedger::Engine::Receipt
