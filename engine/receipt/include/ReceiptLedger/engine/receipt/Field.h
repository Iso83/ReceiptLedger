#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ReceiptLedger::Engine::Receipt {

template <typename T> struct Field {
    std::optional<T> value;
    std::string origin = "unknown";
    std::string raw;
    std::vector<std::string> regions;
    std::string rule;
    std::vector<std::string> alternatives;
};

struct Check {
    std::string status = "not_checkable";
    std::string reason = "insufficient printed data";
    std::optional<std::int64_t> difference;
    static Check compare(std::optional<std::int64_t> calculated, std::optional<std::int64_t> printed);
};

} // namespace ReceiptLedger::Engine::Receipt
