#include "lightning_wave_animation.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace why {
namespace animations {

namespace {
constexpr const char* kDefaultGlyphFilePath = "assets/lightning_wave.txt";
constexpr const char* kDefaultGlyphs = "\u2588\u2593\u2592\u2591 ";
constexpr float kDefaultPersistenceDuration = 0.75f;
constexpr float kDefaultFadeDuration = 1.0f;
constexpr float kDefaultWaveSpeed = 42.0f;
constexpr int kDefaultWaveFrontWidth = 2;
constexpr int kDefaultWaveTailLength = 7;
constexpr float kDefaultActivationSmoothing = 0.12f;

std::vector<std::string> parse_glyphs(const std::string& source) {
    std::vector<std::string> glyphs;
    glyphs.reserve(source.size());

    for (std::size_t i = 0; i < source.size();) {
        unsigned char lead = static_cast<unsigned char>(source[i]);
        std::size_t length = 1;
        if ((lead & 0x80u) == 0x00u) {
            length = 1;
        } else if ((lead & 0xE0u) == 0xC0u && i + 1 < source.size()) {
            length = 2;
        } else if ((lead & 0xF0u) == 0xE0u && i + 2 < source.size()) {
            length = 3;
        } else if ((lead & 0xF8u) == 0xF0u && i + 3 < source.size()) {
            length = 4;
        } else {
            ++i;
            continue;
        }

        glyphs.emplace_back(source.substr(i, length));
        i += length;
    }

    glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(), [](const std::string& g) {
                       return g.empty();
                   }),
                 glyphs.end());

    if (glyphs.empty()) {
        glyphs.emplace_back("#");
    }

    return glyphs;
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

LightningWaveAnimation::LightningWaveAnimation()
    : glyphs_(parse_glyphs(kDefaultGlyphs)),
      glyphs_file_path_(kDefaultGlyphFilePath),
      alternate_direction_(true),
      next_direction_right_(true),
      wave_speed_cols_per_s_(kDefaultWaveSpeed),
      wave_front_width_cols_(kDefaultWaveFrontWidth),
      wave_tail_length_cols_(kDefaultWaveTailLength),
      persistence_duration_s_(kDefaultPersistenceDuration),
      fade_duration_s_(kDefaultFadeDuration),
      trigger_threshold_(0.5f),
      activation_smoothing_s_(kDefaultActivationSmoothing),
      smoothed_energy_(0.0f) {}

LightningWaveAnimation::~LightningWaveAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
        plane_ = nullptr;
    }
}

void LightningWaveAnimation::init(notcurses* nc, const AppConfig& config) {
    if (plane_) {
        ncplane_destroy(plane_);
        plane_ = nullptr;
    }

    glyphs_file_path_ = kDefaultGlyphFilePath;
    glyphs_ = parse_glyphs(kDefaultGlyphs);
    glyphs_loaded_ = false;
    z_index_ = 0;
    is_active_ = false;
    wave_active_ = false;
    wave_direction_right_ = true;
    alternate_direction_ = true;
    next_direction_right_ = true;
    wave_speed_cols_per_s_ = kDefaultWaveSpeed;
    wave_front_width_cols_ = kDefaultWaveFrontWidth;
    wave_tail_length_cols_ = kDefaultWaveTailLength;
    persistence_duration_s_ = kDefaultPersistenceDuration;
    persistence_timer_s_ = 0.0f;
    fade_duration_s_ = kDefaultFadeDuration;
    trigger_band_index_ = -1;
    trigger_threshold_ = 0.5f;
    activation_level_ = 0.0f;
    smoothed_energy_ = 0.0f;

    plane_rows_ = 0;
    plane_cols_ = 0;
    plane_origin_y_ = 0;
    plane_origin_x_ = 0;

    configure_from_app(config);
    create_plane(nc);
    refresh_dimensions();
}

void LightningWaveAnimation::configure_from_app(const AppConfig& config) {
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "LightningWave") {
            z_index_ = anim_config.z_index;
            is_active_ = anim_config.initially_active;
            trigger_band_index_ = anim_config.trigger_band_index;
            if (anim_config.trigger_threshold > 0.0f) {
                trigger_threshold_ = anim_config.trigger_threshold;
            }
            if (!anim_config.glyphs_file_path.empty()) {
                if (glyphs_file_path_ != anim_config.glyphs_file_path) {
                    glyphs_file_path_ = anim_config.glyphs_file_path;
                    glyphs_loaded_ = false;
                }
            }
            if (anim_config.display_duration_s > 0.0f) {
                persistence_duration_s_ = anim_config.display_duration_s;
            }
            if (anim_config.fade_duration_s > 0.0f) {
                fade_duration_s_ = anim_config.fade_duration_s;
            }
            if (anim_config.trigger_cooldown_s > 0.0f) {
                activation_smoothing_s_ = anim_config.trigger_cooldown_s;
            }
            if (anim_config.wave_speed_cols_per_s > 0.0f) {
                wave_speed_cols_per_s_ = anim_config.wave_speed_cols_per_s;
            }
            if (anim_config.wave_front_width_cols > 0) {
                wave_front_width_cols_ = anim_config.wave_front_width_cols;
            }
            if (anim_config.wave_tail_length_cols >= 0) {
                wave_tail_length_cols_ = anim_config.wave_tail_length_cols;
            }
            alternate_direction_ = anim_config.wave_alternate_direction;
            next_direction_right_ = anim_config.wave_direction_right;
            wave_direction_right_ = next_direction_right_;
            if (anim_config.plane_y) {
                plane_origin_y_ = *anim_config.plane_y;
            }
            if (anim_config.plane_x) {
                plane_origin_x_ = *anim_config.plane_x;
            }
            if (anim_config.plane_rows) {
                plane_rows_ = static_cast<unsigned int>(std::max(1, *anim_config.plane_rows));
            }
            if (anim_config.plane_cols) {
                plane_cols_ = static_cast<unsigned int>(std::max(1, *anim_config.plane_cols));
            }
            break;
        }
    }
}

void LightningWaveAnimation::create_plane(notcurses* nc) {
    if (!nc) {
        return;
    }

    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int std_rows = 0;
    unsigned int std_cols = 0;
    ncplane_dim_yx(stdplane, &std_rows, &std_cols);

    if (plane_rows_ == 0u || plane_rows_ > std_rows) {
        plane_rows_ = std_rows;
    }
    if (plane_cols_ == 0u || plane_cols_ > std_cols) {
        plane_cols_ = std_cols;
    }

    plane_origin_y_ = std::clamp(plane_origin_y_, 0, static_cast<int>(std_rows));
    plane_origin_x_ = std::clamp(plane_origin_x_, 0, static_cast<int>(std_cols));

    ncplane_options opts{};
    opts.rows = plane_rows_;
    opts.cols = plane_cols_;
    opts.y = plane_origin_y_;
    opts.x = plane_origin_x_;

    plane_ = ncplane_create(stdplane, &opts);

    ensure_glyphs_loaded();
}

void LightningWaveAnimation::refresh_dimensions() {
    if (!plane_) {
        return;
    }

    unsigned int rows = 0;
    unsigned int cols = 0;
    ncplane_dim_yx(plane_, &rows, &cols);
    if (rows != plane_rows_ || cols != plane_cols_) {
        plane_rows_ = rows;
        plane_cols_ = cols;
    }

    if (columns_.size() != plane_cols_) {
        columns_.assign(plane_cols_, ColumnState{});
    }
}

bool LightningWaveAnimation::load_glyphs_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string contents = buffer.str();
    glyphs_ = parse_glyphs(contents);
    return !glyphs_.empty();
}

void LightningWaveAnimation::ensure_glyphs_loaded() {
    if (glyphs_loaded_) {
        return;
    }
    if (load_glyphs_from_file(glyphs_file_path_)) {
        glyphs_loaded_ = true;
        return;
    }
    if (glyphs_file_path_ != kDefaultGlyphFilePath) {
        if (load_glyphs_from_file(kDefaultGlyphFilePath)) {
            glyphs_loaded_ = true;
            return;
        }
    }
    glyphs_ = parse_glyphs(kDefaultGlyphs);
    glyphs_loaded_ = true;
}

void LightningWaveAnimation::activate() {
    is_active_ = true;
    start_wave();
}

void LightningWaveAnimation::deactivate() {
    is_active_ = false;
    persistence_timer_s_ = persistence_duration_s_;
}

bool LightningWaveAnimation::is_active() const {
    return is_active_ || wave_active_ || persistence_timer_s_ > 0.0f || has_visible_columns();
}

void LightningWaveAnimation::start_wave() {
    if (!plane_ || plane_cols_ == 0u) {
        return;
    }

    ensure_glyphs_loaded();
    refresh_dimensions();

    columns_.assign(plane_cols_, ColumnState{});
    wave_active_ = true;

    bool direction_right = wave_direction_right_;
    if (alternate_direction_) {
        direction_right = next_direction_right_;
        next_direction_right_ = !next_direction_right_;
    }
    wave_direction_right_ = direction_right;

    wave_head_position_ = wave_direction_right_ ? -1.0f : static_cast<float>(plane_cols_);
    persistence_timer_s_ = persistence_duration_s_;
    activation_level_ = 1.0f;
    smoothed_energy_ = clamp01(trigger_threshold_);
}

void LightningWaveAnimation::decay_columns(float delta_time) {
    if (columns_.empty()) {
        return;
    }

    const float fade = (fade_duration_s_ > 0.0f) ? delta_time / fade_duration_s_ : 1.0f;
    for (auto& column : columns_) {
        column.intensity = std::max(0.0f, column.intensity - fade);
    }
}

void LightningWaveAnimation::update_wave(float delta_time) {
    if (!wave_active_) {
        return;
    }

    const float direction = wave_direction_right_ ? 1.0f : -1.0f;
    wave_head_position_ += direction * wave_speed_cols_per_s_ * delta_time;

    const int front_width = std::max(1, wave_front_width_cols_);
    const int tail_segments = std::max(0, wave_tail_length_cols_);
    const int total_segments = std::max(1, front_width + tail_segments);

    for (int segment = 0; segment < total_segments; ++segment) {
        const float offset = static_cast<float>(segment);
        const float column_position = wave_direction_right_ ? wave_head_position_ - offset
                                                            : wave_head_position_ + offset;
        const int column_index = static_cast<int>(std::round(column_position));
        if (column_index < 0 || column_index >= static_cast<int>(plane_cols_)) {
            continue;
        }

        float intensity = 1.0f;
        if (segment >= front_width) {
            const int tail_index = segment - front_width;
            const float denom = static_cast<float>(tail_segments + 1);
            intensity = std::max(0.0f, 1.0f - (static_cast<float>(tail_index) + 1.0f) / denom);
        }

        columns_[static_cast<std::size_t>(column_index)].intensity =
            std::max(columns_[static_cast<std::size_t>(column_index)].intensity, intensity);
    }

    const float trailing_offset = static_cast<float>(total_segments - 1);
    const float trailing_position = wave_direction_right_ ? wave_head_position_ - trailing_offset
                                                          : wave_head_position_ + trailing_offset;

    if (wave_direction_right_) {
        if (trailing_position >= static_cast<float>(plane_cols_)) {
            wave_active_ = false;
        }
    } else {
        if (trailing_position < 0.0f) {
            wave_active_ = false;
        }
    }

    if (!wave_active_) {
        persistence_timer_s_ = persistence_duration_s_;
    }
}

bool LightningWaveAnimation::has_visible_columns() const {
    return std::any_of(columns_.begin(), columns_.end(), [](const ColumnState& column) {
        return column.intensity > 0.01f;
    });
}

void LightningWaveAnimation::update(float delta_time,
                                    const AudioMetrics& /*metrics*/,
                                    const std::vector<float>& bands,
                                    float /*beat_strength*/) {
    if (!plane_) {
        return;
    }

    refresh_dimensions();

    if (persistence_timer_s_ > 0.0f) {
        persistence_timer_s_ = std::max(0.0f, persistence_timer_s_ - delta_time);
    }

    decay_columns(delta_time);

    if (is_active_ && !wave_active_) {
        start_wave();
    }

    if (wave_active_) {
        if (trigger_band_index_ >= 0 && trigger_band_index_ < static_cast<int>(bands.size())) {
            const float target_energy = clamp01(bands[static_cast<std::size_t>(trigger_band_index_)]);
            const float smoothing = std::max(activation_smoothing_s_, 0.01f);
            const float lerp_alpha = std::clamp(delta_time / smoothing, 0.0f, 1.0f);
            smoothed_energy_ = smoothed_energy_ + (target_energy - smoothed_energy_) * lerp_alpha;
            const float threshold = clamp01(trigger_threshold_);
            const float denom = std::max(0.05f, 1.0f - threshold);
            activation_level_ = clamp01((smoothed_energy_ - threshold) / denom);
        } else {
            activation_level_ = 1.0f;
        }

        update_wave(delta_time);
    }
}

void LightningWaveAnimation::render(notcurses* /*nc*/) {
    if (!plane_) {
        return;
    }

    ncplane_erase(plane_);

    if (plane_rows_ == 0u || plane_cols_ == 0u) {
        return;
    }

    ensure_glyphs_loaded();

    ncplane_set_fg_rgb8(plane_, 255, 255, 180);

    const std::size_t glyph_count = glyphs_.empty() ? 0u : glyphs_.size();
    if (glyph_count == 0u) {
        return;
    }

    for (unsigned int col = 0; col < plane_cols_; ++col) {
        const float intensity = clamp01(columns_[col].intensity * std::max(activation_level_, 0.35f));
        if (intensity <= 0.0f) {
            continue;
        }

        const float normalized = 1.0f - intensity;
        int glyph_index = static_cast<int>(std::floor(normalized * static_cast<float>(glyph_count)));
        if (glyph_index < 0) {
            glyph_index = 0;
        } else if (glyph_index >= static_cast<int>(glyph_count)) {
            glyph_index = static_cast<int>(glyph_count) - 1;
        }

        const std::string& glyph = glyphs_[static_cast<std::size_t>(glyph_index)];
        for (unsigned int row = 0; row < plane_rows_; ++row) {
            ncplane_putstr_yx(plane_, row, static_cast<int>(col), glyph.c_str());
        }
    }
}

} // namespace animations
} // namespace why

