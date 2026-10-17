#pragma once

#include <optional>
#include <stdexcept>
#include <string>

namespace ReceiptLedger::Engine::Receipt {

class Decimal {
private:
    std::int64_t m_coefficient = 0;
    int m_scale = 0;

public:
    Decimal(std::int64_t coefficient, int scale) : m_coefficient(coefficient), m_scale(scale) {
        if (scale < 0 || scale > 6)
            throw std::invalid_argument("decimal: scale outside 0..6");
    }

    static std::optional<Decimal> parse(std::string_view text);
    static Decimal read(std::string_view text) {
        const auto value = parse(text);
        if (!value)
            throw std::invalid_argument("decimal: invalid or overflowing source number");

        return *value;
    }
    std::string text(char separator = ',') const;
    std::int64_t cents() const {
        return productCents(Decimal(1, 0), *this);
    }
    static std::int64_t productCents(const Decimal& quantity, const Decimal& unitPrice);
    static std::int64_t add(std::int64_t left, std::int64_t right) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
            (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right))
            throw std::overflow_error("money: sum overflow");

        return left + right;
    }
    static std::int64_t difference(std::int64_t left, std::int64_t right) {
        if (right == std::numeric_limits<std::int64_t>::min())
            throw std::overflow_error("money: difference overflow");

        return add(left, -right);
    }
    static std::int64_t taxCents(std::int64_t net, const Decimal& rate) {
        // net cents × percentage / 100, expressed as a monetary decimal.
        return productCents(Decimal(net, 4), rate);
    }
};

} // namespace ReceiptLedger::Engine::Receipt
