#pragma once

#include <array>
#include <filesystem>

namespace ReceiptLedger::Internal {

class Sha256 {
private:
    std::array<std::uint32_t, 8> m_state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> m_block{};
    std::size_t m_used = 0;
    std::uint64_t m_size = 0;

public:
    void update(const std::uint8_t* data, std::size_t size);
    std::string finish();
    static std::string file(const std::filesystem::path& path);

private:
    void transform();
};

} // namespace ReceiptLedger::Internal
