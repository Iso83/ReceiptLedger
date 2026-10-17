#include <ReceiptLedger/engine/receipt/Decimal.h>
#include <ReceiptLedger/engine/receipt/Field.h>

namespace ReceiptLedger::Engine::Receipt {

Check Check::compare(std::optional<std::int64_t> calculated, std::optional<std::int64_t> printed) {
    if (!calculated || !printed)
        return {};

    const auto delta = Decimal::difference(*calculated, *printed);
    return {delta == 0 ? "match" : "mismatch", "calculated minus independently printed; tolerance 0 cents", delta};
}

} // namespace ReceiptLedger::Engine::Receipt
