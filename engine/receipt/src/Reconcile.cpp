#include <ReceiptLedger/engine/receipt/Interpreter.h>
#include <algorithm>

namespace ReceiptLedger::Engine::Receipt {

void Interpreter::reconcile(Result& receipt) {
    receipt.printedSum.reset();
    receipt.calculatedSum.reset();
    bool allPrinted = !receipt.items.empty();
    bool allCalculated = allPrinted;
    std::int64_t printedSum = 0;
    std::int64_t calculatedSum = 0;
    for (auto* group : {&receipt.items, &receipt.corrections}) {
        for (auto& item : *group) {
            item.calculated.reset();
            if (item.quantity.value && item.unitPrice.value)
                item.calculated = Decimal::productCents(*item.quantity.value, *item.unitPrice.value);

            if (item.calculated && item.discount.value)
                item.calculated = Decimal::add(*item.calculated, *item.discount.value);

            item.amountCheck = Check::compare(item.calculated, item.printed.value);
            if (item.printed.value)
                printedSum = Decimal::add(printedSum, *item.printed.value);
            else
                allPrinted = false;

            if (item.calculated)
                calculatedSum = Decimal::add(calculatedSum, *item.calculated);
            else if (item.kind != "purchase" && item.printed.value)
                calculatedSum = Decimal::add(calculatedSum, *item.printed.value);
            else
                allCalculated = false;
        }
    }
    if (allPrinted)
        receipt.printedSum = printedSum;

    if (allCalculated)
        receipt.calculatedSum = calculatedSum;

    receipt.totalCheck = Check::compare(receipt.printedSum, receipt.total.value);
    receipt.calculatedTotalCheck = Check::compare(receipt.calculatedSum, receipt.total.value);
    bool completeTaxes = !receipt.taxes.empty();
    std::int64_t grossSum = 0;
    const bool itemCodesComplete =
        !receipt.items.empty() && receipt.corrections.empty() &&
        std::ranges::all_of(receipt.items, [](const auto& item) { return item.vatCode.value && item.printed.value; });
    for (auto& tax : receipt.taxes) {
        tax.itemsGrossCheck = {};
        if (itemCodesComplete && tax.code.value) {
            std::int64_t itemGross = 0;
            for (const auto& item : receipt.items) {
                if (item.vatCode.value == tax.code.value)
                    itemGross = Decimal::add(itemGross, *item.printed.value);
            }
            tax.itemsGrossCheck = Check::compare(itemGross, tax.gross.value);
        }

        std::optional<std::int64_t> gross;
        if (tax.net.value && tax.tax.value)
            gross = Decimal::add(*tax.net.value, *tax.tax.value);
        tax.grossCheck = Check::compare(gross, tax.gross.value);

        std::optional<std::int64_t> calculatedTax;
        if (tax.net.value && tax.rate.value)
            calculatedTax = Decimal::taxCents(*tax.net.value, *tax.rate.value);
        tax.rateCheck = Check::compare(calculatedTax, tax.tax.value);

        if (tax.gross.value)
            grossSum = Decimal::add(grossSum, *tax.gross.value);
        else if (gross)
            grossSum = Decimal::add(grossSum, *gross);
        else
            completeTaxes = false;
    }

    receipt.taxTotalCheck = Check::compare(completeTaxes ? std::optional(grossSum) : std::nullopt, receipt.total.value);
    // No financial result changes parser uncertainty or source values.
}

} // namespace ReceiptLedger::Engine::Receipt
