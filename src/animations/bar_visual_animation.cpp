#include "bar_visual_animation.h"

#include <algorithm>
#include <iostream>
#include <cmath>

namespace why {
namespace animations {

const std::string BarVisualAnimation::kAsciiGlyphs = R"( .'`^",:;Il!i><~+_-?][}{1)(|\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$)";

BarVisualAnimation::BarVisualAnimation() = default;

BarVisualAnimation::~BarVisualAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
    }
}

void BarVisualAnimation::init(notcurses* nc, const AppConfig& config) {
    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int std_rows = 0;
    unsigned int std_cols = 0;
    ncplane_dim_yx(stdplane, &std_rows, &std_cols);

    plane_rows_ = std_rows;
    plane_cols_ = std_cols;

    ncplane_options p_opts{};
    p_opts.rows = plane_rows_;
    p_opts.cols = plane_cols_;
    p_opts.y = 0;
    p_opts.x = 0;
    plane_ = ncplane_create(stdplane, &p_opts);
    if (!plane_) {
        std::cerr << "[BarVisualAnimation::init] Failed to create ncplane!" << std::endl;
    } else {
        std::clog << "[BarVisualAnimation::init] ncplane created successfully with dimensions: " << plane_rows_ << "x" << plane_cols_ << std::endl;
    }

    // Set z-index and initial active state from config
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "BarVisual") { // Assuming type is used to identify
            z_index_ = anim_config.z_index;
            is_active_ = anim_config.initially_active;
            break;
        }
    }
}

void BarVisualAnimation::activate() {
    is_active_ = true;
    if (plane_) {
        // Optionally reset or re-draw something on activation
    }
}

void BarVisualAnimation::deactivate() {
    is_active_ = false;
    if (plane_) {
        ncplane_erase(plane_); // Clear the plane when deactivated
    }
}

void BarVisualAnimation::update(float delta_time,
                                const AudioMetrics& metrics,
                                const std::vector<float>& bands,
                                float beat_strength) {
    if (!plane_ || !is_active_) return;

    // Copy bands data for rendering
    current_bands_ = bands;

    // Normalize band energies for visualization
    float max_energy = 0.0f;
    for (float energy : current_bands_) {
        max_energy = std::max(max_energy, energy);
    }

    if (max_energy > 0.0f) {
        for (float& energy : current_bands_) {
            energy /= max_energy;
        }
    }

    // Example: Adjust bar height based on beat strength (simple effect)
    // For a more complex visual, you'd process bands more extensively here.
    if (beat_strength > 0.5f) {
        // Maybe amplify bands temporarily or change color in render
    }
}

void BarVisualAnimation::render(notcurses* nc) {
    if (!plane_ || !is_active_) return;

    ncplane_erase(plane_);

    if (current_bands_.empty()) {
        ncplane_set_fg_rgb8(plane_, 255, 0, 0); // Red for no bands
        ncplane_putstr_yx(plane_, plane_rows_ / 2, plane_cols_ / 2 - 5, "No Audio");
        return;
    }

    const unsigned int num_bands = current_bands_.size();
    if (num_bands == 0) return;

    const unsigned int bar_width = std::max(1u, plane_cols_ / num_bands);
    const unsigned int max_bar_height = plane_rows_ - 1; // Leave some space for metrics

    for (unsigned int i = 0; i < num_bands; ++i) {
        const float band_energy = current_bands_[i];
        // Simple scaling for visualization
        const unsigned int bar_height = static_cast<unsigned int>(std::round(band_energy * max_bar_height));

        // Determine glyph based on energy/height
        const std::size_t glyph_count = kAsciiGlyphs.size();
        const std::size_t glyph_idx = (glyph_count > 1)
                                          ? std::min<std::size_t>(
                                                glyph_count - 1,
                                                static_cast<std::size_t>(std::round(
                                                    band_energy * static_cast<float>(glyph_count - 1))))
                                          : 0;
        const char glyph = kAsciiGlyphs[glyph_idx];

        // Draw each bar
        for (unsigned int h = 0; h < bar_height; ++h) {
            for (unsigned int w = 0; w < bar_width; ++w) {
                ncplane_set_fg_rgb8(plane_, 0, 255, 0); // Green bars
                ncplane_set_bg_rgb8(plane_, 0, 0, 0);   // Black background
                ncplane_putstr_yx(plane_, plane_rows_ - 1 - h, i * bar_width + w, std::string(1, glyph).c_str());
            }
        }
    }
}
} // namespace animations
} // namespace why