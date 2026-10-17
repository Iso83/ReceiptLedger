#include "LedgerReport.h"

#include <fstream>

namespace Receipt = ReceiptLedger::Engine::Receipt;
namespace OCR = ReceiptLedger::Engine::Ocr;

namespace ReceiptLedger::App::PDF2CSV {

nlohmann::json LedgerReport::json(const Receipt::Result& receipt) {
    nlohmann::json result{
        {"id", receipt.id},
        {"store", field(receipt.store)},
        {"date", field(receipt.date)},
        {"time", field(receipt.time)},
        {"currency", field(receipt.currency)},
        {"subtotal_cents", field(receipt.subtotal)},
        {"total_cents", field(receipt.total)},
        {"total_check", check(receipt.totalCheck)},
        {"calculated_total_check", check(receipt.calculatedTotalCheck)},
        {"tax_total_check", check(receipt.taxTotalCheck)},
        {"review_reasons", receipt.reviewReasons},
        {"items", nlohmann::json::array()},
        {"corrections", nlohmann::json::array()},
        {"taxes", nlohmann::json::array()},
        {"rounding", "half away from zero at line cents; tolerance 0 cents; checked int64 arithmetic"}};
    result["printed_sum_cents"] = receipt.printedSum ? nlohmann::json(*receipt.printedSum) : nlohmann::json(nullptr);
    result["calculated_sum_cents"] =
        receipt.calculatedSum ? nlohmann::json(*receipt.calculatedSum) : nlohmann::json(nullptr);
    for (const auto& value : receipt.items)
        result["items"].push_back(item(value));

    for (const auto& value : receipt.corrections)
        result["corrections"].push_back(item(value));

    for (const auto& value : receipt.taxes)
        result["taxes"].push_back({{"code", field(value.code)},
                                   {"rate", field(value.rate)},
                                   {"net_cents", field(value.net)},
                                   {"tax_cents", field(value.tax)},
                                   {"gross_cents", field(value.gross)},
                                   {"gross_check", check(value.grossCheck)},
                                   {"rate_check", check(value.rateCheck)},
                                   {"items_gross_check", check(value.itemsGrossCheck)}});

    return result;
}

nlohmann::json LedgerReport::ocr(const OCR::Result& result, int rotation, const std::string& reason) {
    nlohmann::json output{
        {"rotation_degrees", rotation},
        {"rotation_reason", reason},
        {"text", result.text},
        {"width", result.width},
        {"height", result.height},
        {"regions", nlohmann::json::array()},
        {"field_readings", nlohmann::json::array()},
        {"field_reading_modes", {{"numeric", 7}, {"description", 6}}},
        {"coordinates", "upright input pixels; half-open boxes; original page OCR retained separately"}};
    output["to_source_transform"] =
        rotation == 180 ? nlohmann::json{-1, 0, result.width, 0, -1, result.height} : nlohmann::json{1, 0, 0, 0, 1, 0};
    const auto region = [](const auto& value) {
        return nlohmann::json{
            {"id", value.id},
            {"parent_id", value.parentId},
            {"text", value.text},
            {"level", value.level == OCR::Level::Line   ? "line"
                      : value.level == OCR::Level::Word ? "word"
                                                        : "symbol"},
            {"box", {value.left, value.top, value.right, value.bottom}},
            {"purpose", value.purpose},
            {"confidence", value.confidence ? nlohmann::json(*value.confidence) : nlohmann::json(nullptr)}};
    };
    for (const auto& value : result.regions)
        output["regions"].push_back(region(value));

    for (const auto& value : result.fieldReadings)
        output["field_readings"].push_back(region(value));

    return output;
}

void LedgerReport::writeTemporary(const std::filesystem::path& directory, const std::vector<PageResult>& pages) {
    std::ofstream receipts(directory / ".receipts.csv.tmp", std::ios::binary | std::ios::trunc);
    std::ofstream items(directory / ".items.csv.tmp", std::ios::binary | std::ios::trunc);
    if (!receipts || !items)
        throw std::runtime_error("output: could not create ledger reports");

    const bool complete = std::ranges::all_of(pages, [](const auto& page) { return page.status == "complete"; });
    receipts.imbue(std::locale::classic());
    items.imbue(std::locale::classic());
    receipts << "\xEF\xBB\xBF"
                "receipt-id;document-id;source;page;store;date;time;currency;printed-total;printed-item-sum;calculated-"
                "item-sum;total-status;total-difference;calculated-total-status;vat-status;vat-difference;review-"
                "reasons;run-status;vat-net;vat-amount;vat-gross;vat-groups\n";
    items << "\xEF\xBB\xBF"
             "item-id;receipt-id;document-id;source;page;description;quantity;unit;unit-price;printed-amount;"
             "calculated-amount;discount;vat-code;vat-rate;amount-status;amount-difference;source-regions;review-"
             "reasons;run-status;printed-code\n";
    for (const auto& page : pages) {
        if (!page.receipt)
            continue;

        const auto& receipt = *page.receipt;
        const auto stringValue = [](const auto& value) { return value.value.value_or(""); };
        const auto decimalValue = [](const auto& value) { return value.value ? value.value->text() : std::string{}; };
        const auto taxSum = [&](const auto& member) -> std::optional<std::int64_t> {
            if (receipt.taxes.empty())
                return std::nullopt;

            std::int64_t sum = 0;
            for (const auto& tax : receipt.taxes) {
                if (!(tax.*member).value)
                    return std::nullopt;

                sum = Engine::Receipt::Decimal::add(sum, *(tax.*member).value);
            }
            return sum;
        };
        receipts << quote(receipt.id) << ';' << quote(page.documentId) << ';' << quote(page.source.string()) << ';'
                 << page.page << ';' << quote(stringValue(receipt.store)) << ';' << quote(stringValue(receipt.date))
                 << ';' << quote(stringValue(receipt.time)) << ';' << quote(stringValue(receipt.currency)) << ';'
                 << money(receipt.total.value) << ';' << money(receipt.printedSum) << ';'
                 << money(receipt.calculatedSum) << ';' << receipt.totalCheck.status << ';'
                 << money(receipt.totalCheck.difference) << ';' << receipt.calculatedTotalCheck.status << ';'
                 << receipt.taxTotalCheck.status << ';' << money(receipt.taxTotalCheck.difference) << ';'
                 << quote(nlohmann::json(receipt.reviewReasons).dump()) << ';' << (complete ? "complete" : "failed")
                 << ';' << money(taxSum(&Engine::Receipt::Tax::net)) << ';' << money(taxSum(&Engine::Receipt::Tax::tax))
                 << ';' << money(taxSum(&Engine::Receipt::Tax::gross)) << ';' << quote(json(receipt)["taxes"].dump())
                 << '\n';

        for (const auto& value : receipt.items) {
            std::vector<std::string> regions;
            for (const auto* fieldRegions : {&value.description.regions, &value.quantity.regions,
                                             &value.unitPrice.regions, &value.printed.regions})
                regions.insert(regions.end(), fieldRegions->begin(), fieldRegions->end());

            items << quote(value.id) << ';' << quote(receipt.id) << ';' << quote(page.documentId) << ';'
                  << quote(page.source.string()) << ';' << page.page << ';' << quote(stringValue(value.description))
                  << ';' << decimalValue(value.quantity) << ';' << quote(stringValue(value.unit)) << ';'
                  << decimalValue(value.unitPrice) << ';' << money(value.printed.value) << ';'
                  << money(value.calculated) << ';' << money(value.discount.value) << ';'
                  << quote(stringValue(value.vatCode)) << ';' << decimalValue(value.vatRate) << ';'
                  << value.amountCheck.status << ';' << money(value.amountCheck.difference) << ';'
                  << quote(nlohmann::json(regions).dump()) << ';' << quote(nlohmann::json(value.reviewReasons).dump())
                  << ';' << (complete ? "complete" : "failed") << ';' << quote(stringValue(value.marker)) << '\n';
        }
    }
    receipts.flush();
    items.flush();
    if (!receipts || !items)
        throw std::runtime_error("output: could not finish ledger reports");

    receipts.close();
    items.close();
    if (!receipts || !items)
        throw std::runtime_error("output: could not close ledger reports");
}

std::string LedgerReport::quote(const std::string& text) {
    std::string result = "\"";
    if (!text.empty() && (text.front() == '=' || text.front() == '+' || text.front() == '-' || text.front() == '@'))
        result += '\'';

    for (const auto character : text) {
        result += character;
        if (character == '"')
            result += '"';
    }

    return result + '"';
}

} // namespace ReceiptLedger::App::PDF2CSV
