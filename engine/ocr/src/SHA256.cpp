#include "SHA256.h"

#include <bit>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ReceiptLedger::Internal {

void Sha256::update(const std::uint8_t* data, std::size_t size) {
    m_size += size;
    while (size > 0) {
        const std::size_t amount = std::min(size, m_block.size() - m_used);
        std::copy_n(data, amount, m_block.begin() + static_cast<std::ptrdiff_t>(m_used));
        m_used += amount;
        data += amount;
        size -= amount;
        if (m_used == m_block.size()) {
            transform();
            m_used = 0;
        }
    }
}

std::string Sha256::finish() {
    const std::uint64_t bits = m_size * 8;
    m_block[m_used++] = 0x80;
    if (m_used > 56) {
        std::fill(m_block.begin() + static_cast<std::ptrdiff_t>(m_used), m_block.end(), 0);
        transform();
        m_used = 0;
    }
    std::fill(m_block.begin() + static_cast<std::ptrdiff_t>(m_used), m_block.begin() + 56, 0);
    for (int index = 0; index < 8; ++index)
        m_block[63 - index] = static_cast<std::uint8_t>(bits >> (index * 8));

    transform();
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto value : m_state)
        result << std::setw(8) << value;

    return result.str();
}

std::string Sha256::file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("model: could not read language data: " + path.string());

    Sha256 hash;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        hash.update(reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(input.gcount()));
    }
    if (!input.eof())
        throw std::runtime_error("model: failed while reading language data: " + path.string());

    return hash.finish();
}

void Sha256::transform() {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    std::array<std::uint32_t, 64> words{};
    for (int index = 0; index < 16; ++index)
        words[index] = (static_cast<std::uint32_t>(m_block[index * 4]) << 24) |
                       (static_cast<std::uint32_t>(m_block[index * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(m_block[index * 4 + 2]) << 8) | m_block[index * 4 + 3];

    for (int index = 16; index < 64; ++index) {
        const auto s0 = std::rotr(words[index - 15], 7) ^ std::rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
        const auto s1 = std::rotr(words[index - 2], 17) ^ std::rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }

    auto [a, b, c, d, e, f, g, h] = m_state;
    for (int index = 0; index < 64; ++index) {
        const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const auto choice = (e & f) ^ (~e & g);
        const auto temporary1 = h + sum1 + choice + constants[index] + words[index];
        const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const auto majority = (a & b) ^ (a & c) ^ (b & c);
        const auto temporary2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
}

} // namespace ReceiptLedger::Internal
