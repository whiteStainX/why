#pragma once

#include <string>
#include <vector>

#include "animation.h"

namespace why {
namespace animations {

class LightningWaveAnimation : public Animation {
public:
    LightningWaveAnimation();
    ~LightningWaveAnimation() override;

    void init(notcurses* nc, const AppConfig& config) override;
    void update(float delta_time,
                const AudioMetrics& metrics,
                const std::vector<float>& bands,
                float beat_strength) override;
    void render(notcurses* nc) override;

    void activate() override;
    void deactivate() override;

    bool is_active() const override;
    int get_z_index() const override { return z_index_; }
    ncplane* get_plane() const override { return plane_; }

    void bind_events(const AnimationConfig& config, events::EventBus& bus) override;

private:
    struct ColumnState {
        float intensity = 0.0f;
    };

    struct SpectralSnapshot {
        std::vector<float> distribution;
        float total_energy = 0.0f;
        float centroid = 0.0f;
        float flatness = 0.0f;
        float crest = 0.0f;
    };

    void configure_from_app(const AppConfig& config);
    void create_plane(notcurses* nc);
    void refresh_dimensions();
    bool load_glyphs_from_file(const std::string& path);
    void ensure_glyphs_loaded();

    void start_wave(float intensity);
    void update_wave(float delta_time);
    void decay_columns(float delta_time);
    bool has_visible_columns() const;
    void update_activation_decay(float delta_time);

    SpectralSnapshot analyze_spectrum(const std::vector<float>& bands) const;
    float compute_js_divergence(const std::vector<float>& current,
                                const std::vector<float>& previous) const;
    float compute_flux(const std::vector<float>& current,
                       const std::vector<float>& previous) const;
    bool evaluate_novelty(const SpectralSnapshot& snapshot,
                          float delta_time,
                          float& out_strength);
    void reset_spectral_history();

    ncplane* plane_ = nullptr;
    unsigned int plane_rows_ = 0;
    unsigned int plane_cols_ = 0;
    int plane_origin_y_ = 0;
    int plane_origin_x_ = 0;

    std::vector<ColumnState> columns_;
    std::vector<std::string> glyphs_;
    std::string glyphs_file_path_;
    bool glyphs_loaded_ = false;

    int z_index_ = 0;
    bool is_active_ = false;
    bool wave_active_ = false;
    bool wave_direction_right_ = true;
    bool alternate_direction_ = true;
    bool next_direction_right_ = true;

    float wave_head_position_ = 0.0f;
    float wave_speed_cols_per_s_ = 40.0f;
    int wave_front_width_cols_ = 2;
    int wave_tail_length_cols_ = 6;

    float persistence_duration_s_ = 0.6f;
    float persistence_timer_s_ = 0.0f;
    float fade_duration_s_ = 1.0f;

    int trigger_band_index_ = -1;
    float trigger_threshold_ = 0.5f;
    float activation_level_ = 0.0f;
    float novelty_threshold_ = 0.35f;
    float detection_energy_floor_ = 0.015f;
    float detection_cooldown_s_ = 0.65f;
    float detection_cooldown_timer_s_ = 0.0f;
    float novelty_smoothing_s_ = 0.18f;
    float novelty_smoothed_ = 0.0f;
    float activation_decay_s_ = 0.8f;

    std::vector<float> previous_distribution_;
    float previous_centroid_ = 0.0f;
    float previous_flatness_ = 0.0f;
    float previous_crest_ = 0.0f;
    bool has_previous_signature_ = false;
};

} // namespace animations
} // namespace why

