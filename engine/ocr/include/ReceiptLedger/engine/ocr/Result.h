#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ReceiptLedger::Engine::Ocr {

enum class Level { Line, Word, Symbol };

struct Region {
    std::string id;
    Level level = Level::Word;
    std::string parentId;
    int readingOrder = 0;
    std::string text;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::optional<float> confidence;
    std::string purpose;
};

struct Result {
    std::string text;
    int width = 0;
    int height = 0;
    std::vector<Region> regions;
    std::vector<Region> fieldReadings;
};

} // namespace ReceiptLedger::Engine::Ocr
