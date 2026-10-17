#pragma once

#include <ReceiptLedger/engine/receipt/Decimal.h>
#include <ReceiptLedger/engine/receipt/Field.h>

namespace ReceiptLedger::Engine::Receipt {

struct Item {
    std::string id;
    std::string kind = "purchase";
    Field<std::string> description;
    Field<Decimal> quantity;
    Field<std::string> unit;
    Field<Decimal> unitPrice;
    Field<std::int64_t> printed;
    Field<std::int64_t> discount;
    Field<std::string> marker;
    Field<std::string> vatCode;
    Field<Decimal> vatRate;
    std::optional<std::int64_t> calculated;
    Check amountCheck;
    std::vector<std::string> reviewReasons;
};

} // namespace ReceiptLedger::Engine::Receipt
