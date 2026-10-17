#pragma once

#include <ReceiptLedger/engine/ocr/Result.h>
#include <ReceiptLedger/engine/receipt/Field.h>
#include <regex>
#include <string>
#include <vector>

namespace ReceiptLedger::Internal::Receipt {

struct Row {
    std::string text;
    std::vector<std::string> regions;
    int top = 0;
    int bottom = 0;
    bool uncertain = false;
    std::vector<Engine::Ocr::Region> words;
    std::vector<Engine::Ocr::Region> fields;
    std::vector<Engine::Ocr::Region> symbols;
};

class Layout {
public:
    static std::vector<Row> rows(const Engine::Ocr::Result& source);
    static std::string columnText(const Row& row, int left, int right);
    static std::string trim(std::string text) {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};

        return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    }
    static std::string upper(std::string text) {
        std::ranges::transform(text, text.begin(), [](unsigned char value) { return std::toupper(value); });
        return text;
    }
    template <typename T>
    static Engine::Receipt::Field<T> printed(const T& value, const Row& row, const std::string& rule) {
        return {value, "printed", row.text, row.regions, rule};
    }
};

} // namespace ReceiptLedger::Internal::Receipt
