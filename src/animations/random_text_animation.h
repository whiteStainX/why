#pragma once

#include <random>
#include <string>
#include <chrono>

#include "animation.h"
#include "../config.h" // Explicitly include AppConfig

namespace why {
namespace animations {

class RandomTextAnimation : public Animation {
public:
    RandomTextAnimation();
    ~RandomTextAnimation() override;

    void init(notcurses* nc, const AppConfig& config) override;
    void update(float delta_time,
                const AudioMetrics& metrics,
                const std::vector<float>& bands,
                float beat_strength) override;
    void render(notcurses* nc) override;

    void activate() override;
    void deactivate() override;

    bool is_active() const override { return is_active_; } // Use internal state
    int get_z_index() const override { return z_index_; }
    ncplane* get_plane() const override { return plane_; }

private:
    std::mt19937_64 rng_;
    std::uniform_int_distribution<std::string::size_type> dist_;
    static const std::string chars_;
    ncplane* plane_ = nullptr;
    int z_index_ = 0;
    bool is_active_ = true; // New: internal active state
    std::string current_text_;
};

} // namespace animations
} // namespace why