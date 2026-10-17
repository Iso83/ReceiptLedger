#pragma once

#include <ReceiptLedger/engine/receipt/Decimal.h>
#include <ReceiptLedger/engine/receipt/Field.h>

namespace ReceiptLedger::Engine::Receipt {

struct Tax {
    Field<std::string> code;
    Field<Decimal> rate;
    Field<std::int64_t> net;
    Field<std::int64_t> tax;
    Field<std::int64_t> gross;
    Check grossCheck;
    Check rateCheck;
    Check itemsGrossCheck;
};

} // namespace ReceiptLedger::Engine::Receipt
