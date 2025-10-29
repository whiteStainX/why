#pragma once

#include <chrono>
#include <random>
#include <string>
#include <vector>

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
    struct DisplayedLine {
        std::string text;
        std::vector<std::string> words;
        std::size_t current_word_index = 0;
        float time_since_last_word = 0.0f;
        bool completed = false;
        float display_elapsed = 0.0f;
        float fade_elapsed = 0.0f;
        bool fading_out = false;
        int y_pos = 0;
    };

    void load_quotes();
    void spawn_line();
    void update_line_positions();
    std::string select_random_quote();

    std::mt19937_64 rng_;
    std::uniform_int_distribution<std::size_t> quote_dist_;
    std::vector<std::string> quotes_;
    std::vector<DisplayedLine> active_lines_;
    ncplane* plane_ = nullptr;
    int z_index_ = 0;
    bool is_active_ = true; // New: internal active state
    int trigger_band_index_ = -1;
    float trigger_threshold_ = 0.0f;
    float trigger_beat_min_ = 0.0f;
    float trigger_beat_max_ = 1.0f;
    float type_speed_words_per_s_ = 4.0f;
    float display_duration_s_ = 3.0f;
    float fade_duration_s_ = 1.0f;
    float trigger_cooldown_s_ = 0.75f;
    int max_active_lines_ = 4;
    float time_since_last_trigger_ = 0.0f;
    std::string text_file_path_ = "assets/dune.txt";
    static constexpr int kBottomMargin = 2;
    static constexpr int kLineSpacing = 2;
};

} // namespace animations
} // namespace why