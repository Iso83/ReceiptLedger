#include <ReceiptLedger/engine/receipt/Decimal.h>
#include <limits>

namespace ReceiptLedger::Engine::Receipt {

std::optional<Decimal> Decimal::parse(std::string_view text) {
    if (text.empty())
        return std::nullopt;

    bool negative = text.front() == '-';
    std::size_t index = negative || text.front() == '+' ? 1 : 0;
    std::int64_t coefficient = 0;
    int scale = 0;
    bool fraction = false;
    bool digit = false;
    for (; index < text.size(); ++index) {
        const char character = text[index];
        if (character == ',' || character == '.') {
            if (fraction)
                return std::nullopt;

            fraction = true;
            continue;
        }

        if (character < '0' || character > '9')
            return std::nullopt;

        digit = true;
        if (fraction && ++scale > 6)
            return std::nullopt;

        if (coefficient > (std::numeric_limits<std::int64_t>::max() - (character - '0')) / 10)
            return std::nullopt;

        coefficient = coefficient * 10 + character - '0';
    }
    if (!digit || (fraction && scale == 0))
        return std::nullopt;

    return Decimal(negative ? -coefficient : coefficient, scale);
}

std::string Decimal::text(char separator) const {
    auto digits = std::to_string(m_coefficient);
    const bool negative = m_coefficient < 0;
    if (negative)
        digits.erase(0, 1);

    if (m_scale) {
        if (digits.size() <= static_cast<std::size_t>(m_scale))
            digits.insert(0, m_scale + 1 - digits.size(), '0');

        digits.insert(digits.size() - m_scale, 1, separator);
    }

    return (negative ? "-" : "") + digits;
}

std::int64_t Decimal::productCents(const Decimal& quantity, const Decimal& unitPrice) {
    const auto left = quantity.m_coefficient;
    const auto right = unitPrice.m_coefficient;
    const auto limit = std::numeric_limits<std::int64_t>::max();
    if (left == std::numeric_limits<std::int64_t>::min() || right == std::numeric_limits<std::int64_t>::min())
        throw std::overflow_error("decimal: unsupported minimum coefficient");

    const auto absoluteLeft = left < 0 ? -left : left;
    const auto absoluteRight = right < 0 ? -right : right;
    if (absoluteRight && absoluteLeft > limit / absoluteRight)
        throw std::overflow_error("decimal: multiplication overflow");

    auto value = absoluteLeft * absoluteRight;
    const int scale = quantity.m_scale + unitPrice.m_scale;
    if (scale < 2) {
        for (int digit = scale; digit < 2; ++digit) {
            if (value > limit / 10)
                throw std::overflow_error("decimal: cents overflow");

            value *= 10;
        }
    } else {
        std::int64_t divisor = 1;
        for (int digit = 2; digit < scale; ++digit)
            divisor *= 10;

        const auto remainder = value % divisor;
        value /= divisor;
        if (remainder >= (divisor + 1) / 2 && divisor > 1)
            ++value;
    }

    return (left < 0) != (right < 0) ? -value : value;
}

} // namespace ReceiptLedger::Engine::Receipt
