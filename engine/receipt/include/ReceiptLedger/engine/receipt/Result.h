#pragma once

#include <ReceiptLedger/engine/receipt/Item.h>
#include <ReceiptLedger/engine/receipt/Tax.h>

namespace ReceiptLedger::Engine::Receipt {

struct Result {
    std::string id;
    Field<std::string> store;
    Field<std::string> date;
    Field<std::string> time;
    Field<std::string> currency;
    Field<std::int64_t> subtotal;
    Field<std::int64_t> total;
    std::vector<Item> items;
    std::vector<Item> corrections;
    std::vector<Tax> taxes;
    std::optional<std::int64_t> printedSum;
    std::optional<std::int64_t> calculatedSum;
    Check totalCheck;
    Check calculatedTotalCheck;
    Check taxTotalCheck;
    std::vector<std::string> reviewReasons;
};

} // namespace ReceiptLedger::Engine::Receipt
