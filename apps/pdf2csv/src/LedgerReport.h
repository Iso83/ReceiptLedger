#pragma once

#include "Report.h"

#include <nlohmann/json.hpp>

namespace ReceiptLedger::App::PDF2CSV {

class LedgerReport {
public:
    static nlohmann::json json(const Engine::Receipt::Result& receipt);
    static nlohmann::json ocr(const Engine::Ocr::Result& result, int rotation, const std::string& reason);
    static void writeTemporary(const std::filesystem::path& directory, const std::vector<PageResult>& pages);

private:
    static std::string quote(const std::string& text);
    static std::string money(const std::optional<std::int64_t>& cents) {
        if (!cents)
            return {};

        return Engine::Receipt::Decimal(*cents, 2).text();
    }
    static nlohmann::json check(const Engine::Receipt::Check& value) {
        return {{"status", value.status},
                {"reason", value.reason},
                {"difference_cents", value.difference ? nlohmann::json(*value.difference) : nlohmann::json(nullptr)},
                {"tolerance_cents", 0}};
    }
    static nlohmann::json item(const Engine::Receipt::Item& value) {
        return {{"id", value.id},
                {"kind", value.kind},
                {"description", field(value.description)},
                {"quantity", field(value.quantity)},
                {"unit", field(value.unit)},
                {"unit_price", field(value.unitPrice)},
                {"printed_cents", field(value.printed)},
                {"calculated_cents", value.calculated ? nlohmann::json(*value.calculated) : nlohmann::json(nullptr)},
                {"printed_code", field(value.marker)},
                {"discount_cents", field(value.discount)},
                {"vat_code", field(value.vatCode)},
                {"vat_rate", field(value.vatRate)},
                {"amount_check", check(value.amountCheck)},
                {"review_reasons", value.reviewReasons}};
    }

    template <typename T> static nlohmann::json field(const Engine::Receipt::Field<T>& value) {
        nlohmann::json result{{"origin", value.origin},
                              {"raw", value.raw},
                              {"regions", value.regions},
                              {"rule", value.rule},
                              {"alternatives", value.alternatives}};
        result["value"] = nullptr;
        if (value.value) {
            if constexpr (std::is_same_v<T, Engine::Receipt::Decimal>)
                result["value"] = value.value->text('.');
            else
                result["value"] = *value.value;
        }

        return result;
    }
};

} // namespace ReceiptLedger::App::PDF2CSV
