#include "lightning_wave_animation.h"
#include "animation_event_utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
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
constexpr float kDefaultNoveltySmoothing = 0.18f;
constexpr float kDefaultNoveltyThreshold = 0.35f;
constexpr float kDefaultEnergyFloor = 0.015f;
constexpr float kDefaultDetectionCooldown = 0.65f;
constexpr float kDefaultActivationDecay = 0.8f;
constexpr float kInvLn2 = 1.4426950408889634074f; // 1 / ln(2)
constexpr float kEnergyEpsilon = 1e-6f;

constexpr float kWeightJensenShannon = 0.5f;
constexpr float kWeightFlux = 0.25f;
constexpr float kWeightCentroid = 0.15f;
constexpr float kWeightFlatness = 0.06f;
constexpr float kWeightCrest = 0.04f;

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
      activation_level_(0.0f),
      novelty_threshold_(kDefaultNoveltyThreshold),
      detection_energy_floor_(kDefaultEnergyFloor),
      detection_cooldown_s_(kDefaultDetectionCooldown),
      detection_cooldown_timer_s_(0.0f),
      novelty_smoothing_s_(kDefaultNoveltySmoothing),
      novelty_smoothed_(0.0f),
      activation_decay_s_(kDefaultActivationDecay),
      previous_centroid_(0.0f),
      previous_flatness_(0.0f),
      previous_crest_(0.0f),
      has_previous_signature_(false) {}

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
    novelty_threshold_ = kDefaultNoveltyThreshold;
    detection_energy_floor_ = kDefaultEnergyFloor;
    detection_cooldown_s_ = kDefaultDetectionCooldown;
    detection_cooldown_timer_s_ = 0.0f;
    novelty_smoothing_s_ = kDefaultNoveltySmoothing;
    novelty_smoothed_ = 0.0f;
    activation_decay_s_ = kDefaultActivationDecay;
    reset_spectral_history();

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
            novelty_threshold_ = std::clamp(anim_config.lightning_novelty_threshold, 0.01f, 1.0f);
            detection_energy_floor_ = std::max(anim_config.lightning_energy_floor, 0.0f);
            detection_cooldown_s_ = std::max(anim_config.lightning_detection_cooldown_s, 0.0f);
            novelty_smoothing_s_ = std::max(anim_config.lightning_novelty_smoothing_s, 0.01f);
            activation_decay_s_ = std::max(anim_config.lightning_activation_decay_s, 0.01f);
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
            if (anim_config.trigger_cooldown_s > 0.0f) {
                novelty_smoothing_s_ = anim_config.trigger_cooldown_s;
                detection_cooldown_s_ = std::max(detection_cooldown_s_, anim_config.trigger_cooldown_s);
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
    reset_spectral_history();
    novelty_smoothed_ = 0.0f;
    detection_cooldown_timer_s_ = 0.0f;
}

void LightningWaveAnimation::deactivate() {
    is_active_ = false;
    persistence_timer_s_ = persistence_duration_s_;
    reset_spectral_history();
}

bool LightningWaveAnimation::is_active() const {
    return is_active_ || wave_active_ || persistence_timer_s_ > 0.0f || has_visible_columns();
}

void LightningWaveAnimation::start_wave(float intensity) {
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
    activation_level_ = clamp01(intensity);
    novelty_smoothed_ = activation_level_;
    detection_cooldown_timer_s_ = detection_cooldown_s_;
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

void LightningWaveAnimation::update_activation_decay(float delta_time) {
    if (activation_level_ <= 0.0f) {
        activation_level_ = 0.0f;
        return;
    }

    if (activation_decay_s_ <= 0.0f) {
        activation_level_ = 0.0f;
        return;
    }

    const float decay = delta_time / activation_decay_s_;
    activation_level_ = std::max(0.0f, activation_level_ - decay);
}

LightningWaveAnimation::SpectralSnapshot
LightningWaveAnimation::analyze_spectrum(const std::vector<float>& bands) const {
    SpectralSnapshot snapshot;
    const std::size_t count = bands.size();
    snapshot.distribution.resize(count, 0.0f);
    if (count == 0) {
        return snapshot;
    }

    float total_energy = 0.0f;
    float log_sum = 0.0f;
    float max_energy = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float energy = std::max(bands[i], 0.0f);
        snapshot.distribution[i] = energy;
        total_energy += energy;
        max_energy = std::max(max_energy, energy);
        log_sum += std::log(std::max(energy, kEnergyEpsilon));
    }

    snapshot.total_energy = total_energy;
    if (total_energy <= kEnergyEpsilon) {
        std::fill(snapshot.distribution.begin(), snapshot.distribution.end(), 0.0f);
        snapshot.centroid = 0.0f;
        snapshot.flatness = 0.0f;
        snapshot.crest = 0.0f;
        return snapshot;
    }

    const float inv_total = 1.0f / total_energy;
    float centroid = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        snapshot.distribution[i] *= inv_total;
        centroid += snapshot.distribution[i] * static_cast<float>(i);
    }
    snapshot.centroid = centroid;

    const float band_count = static_cast<float>(count);
    const float arithmetic_mean = total_energy / band_count;
    const float geometric_mean = std::exp(log_sum / band_count);
    snapshot.flatness = (arithmetic_mean > kEnergyEpsilon)
                            ? std::clamp(geometric_mean / arithmetic_mean, 0.0f, 1.0f)
                            : 0.0f;

    const float crest_ratio =
        (arithmetic_mean > kEnergyEpsilon) ? max_energy / (arithmetic_mean + kEnergyEpsilon) : 0.0f;
    snapshot.crest = std::clamp(std::tanh(std::max(0.0f, crest_ratio - 1.0f) * 0.35f), 0.0f, 1.0f);

    return snapshot;
}

float LightningWaveAnimation::compute_js_divergence(const std::vector<float>& current,
                                                    const std::vector<float>& previous) const {
    const std::size_t size = std::min(current.size(), previous.size());
    if (size == 0) {
        return 0.0f;
    }

    float jsd = 0.0f;
    for (std::size_t i = 0; i < size; ++i) {
        const float p = std::clamp(current[i], kEnergyEpsilon, 1.0f);
        const float q = std::clamp(previous[i], kEnergyEpsilon, 1.0f);
        const float m = 0.5f * (p + q);
        jsd += 0.5f * (p * (std::log(p) - std::log(m)) + q * (std::log(q) - std::log(m)));
    }

    jsd = std::max(jsd * kInvLn2, 0.0f);
    return std::clamp(jsd, 0.0f, 1.0f);
}

float LightningWaveAnimation::compute_flux(const std::vector<float>& current,
                                           const std::vector<float>& previous) const {
    const std::size_t size = std::min(current.size(), previous.size());
    float flux = 0.0f;
    for (std::size_t i = 0; i < size; ++i) {
        const float diff = current[i] - previous[i];
        if (diff > 0.0f) {
            flux += diff;
        }
    }
    return flux;
}

bool LightningWaveAnimation::evaluate_novelty(const SpectralSnapshot& snapshot,
                                              float delta_time,
                                              float& out_strength) {
    if (snapshot.distribution.empty()) {
        out_strength = 0.0f;
        return false;
    }

    if (!has_previous_signature_) {
        previous_distribution_ = snapshot.distribution;
        previous_centroid_ = snapshot.centroid;
        previous_flatness_ = snapshot.flatness;
        previous_crest_ = snapshot.crest;
        has_previous_signature_ = true;
        out_strength = novelty_smoothed_;
        return false;
    }

    const float jsd = compute_js_divergence(snapshot.distribution, previous_distribution_);
    const float flux = compute_flux(snapshot.distribution, previous_distribution_);
    const float centroid_norm = std::abs(snapshot.centroid - previous_centroid_) /
                                std::max(1.0f, static_cast<float>(snapshot.distribution.size() - 1));
    const float flatness_diff = std::abs(snapshot.flatness - previous_flatness_);
    const float crest_diff = std::abs(snapshot.crest - previous_crest_);
    const float flux_norm = std::clamp(flux * 0.5f, 0.0f, 1.0f);

    const float novelty_raw = std::clamp(kWeightJensenShannon * jsd +
                                             kWeightFlux * flux_norm +
                                             kWeightCentroid * centroid_norm +
                                             kWeightFlatness * flatness_diff +
                                             kWeightCrest * crest_diff,
                                         0.0f,
                                         1.0f);

    const float smoothing = std::max(novelty_smoothing_s_, 0.01f);
    const float lerp_alpha = std::clamp(delta_time / smoothing, 0.0f, 1.0f);
    novelty_smoothed_ = std::clamp(novelty_smoothed_ + (novelty_raw - novelty_smoothed_) * lerp_alpha, 0.0f, 1.0f);

    previous_distribution_ = snapshot.distribution;
    previous_centroid_ = snapshot.centroid;
    previous_flatness_ = snapshot.flatness;
    previous_crest_ = snapshot.crest;
    has_previous_signature_ = true;

    out_strength = novelty_smoothed_;
    return novelty_smoothed_ >= novelty_threshold_;
}

void LightningWaveAnimation::reset_spectral_history() {
    previous_distribution_.clear();
    previous_centroid_ = 0.0f;
    previous_flatness_ = 0.0f;
    previous_crest_ = 0.0f;
    has_previous_signature_ = false;
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

    if (detection_cooldown_timer_s_ > 0.0f) {
        detection_cooldown_timer_s_ = std::max(0.0f, detection_cooldown_timer_s_ - delta_time);
    }

    if (is_active_) {
        SpectralSnapshot snapshot = analyze_spectrum(bands);
        if (snapshot.total_energy >= detection_energy_floor_) {
            float novelty_strength = 0.0f;
            const bool novelty_hit = evaluate_novelty(snapshot, delta_time, novelty_strength);
            activation_level_ = std::max(activation_level_, clamp01(novelty_strength));
            if (novelty_hit && detection_cooldown_timer_s_ <= 0.0f) {
                const float boosted_intensity = std::clamp(novelty_strength * 1.15f, 0.35f, 1.0f);
                start_wave(boosted_intensity);
            }
        } else {
            reset_spectral_history();
            const float smoothing = std::max(novelty_smoothing_s_, 0.01f);
            const float lerp_alpha = std::clamp(delta_time / smoothing, 0.0f, 1.0f);
            novelty_smoothed_ = std::clamp(novelty_smoothed_ + (0.0f - novelty_smoothed_) * lerp_alpha, 0.0f, 1.0f);
        }
    }

    if (wave_active_) {
        update_wave(delta_time);
    }

    update_activation_decay(delta_time);
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

void LightningWaveAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    bind_standard_frame_updates(this, config, bus);
}

} // namespace animations
} // namespace why

