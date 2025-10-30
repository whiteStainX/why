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

    void configure_from_app(const AppConfig& config);
    void create_plane(notcurses* nc);
    void refresh_dimensions();
    bool load_glyphs_from_file(const std::string& path);
    void ensure_glyphs_loaded();

    void start_wave();
    void update_wave(float delta_time);
    void decay_columns(float delta_time);
    bool has_visible_columns() const;

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
    float activation_smoothing_s_ = 0.12f;

    float smoothed_energy_ = 0.0f;
};

} // namespace animations
} // namespace why

