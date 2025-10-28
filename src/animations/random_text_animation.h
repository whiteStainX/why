#pragma once

#include <random>
#include <string>
#include <chrono>

#include "animation.h"

namespace why {
namespace animations {

class RandomTextAnimation : public Animation {
public:
    RandomTextAnimation();
    void render(notcurses* nc,
                int grid_rows,
                int grid_cols,
                float time_s,
                float sensitivity,
                const AudioMetrics& metrics,
                const std::vector<float>& bands,
                float beat_strength) override;

private:
    std::mt19937_64 rng_;
    std::uniform_int_distribution<std::string::size_type> dist_;
    static const std::string chars_;
};

} // namespace animations
} // namespace why