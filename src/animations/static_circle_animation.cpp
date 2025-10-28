#include "static_circle_animation.h"

#include <iostream>
#include <cmath>

namespace why {
namespace animations {

StaticCircleAnimation::StaticCircleAnimation() = default;

StaticCircleAnimation::~StaticCircleAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
    }
}

void StaticCircleAnimation::init(notcurses* nc, const AppConfig& config) {
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
        std::cerr << "[StaticCircleAnimation::init] Failed to create ncplane!" << std::endl;
    } else {
        std::clog << "[StaticCircleAnimation::init] ncplane created successfully with dimensions: " << plane_rows_ << "x" << plane_cols_ << std::endl;
    }

    // Set z-index from config if available
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "StaticCircle") { // Assuming type is used to identify
            z_index_ = anim_config.z_index;
            break;
        }
    }
}

void StaticCircleAnimation::update(float delta_time,
                                  const AudioMetrics& metrics,
                                  const std::vector<float>& bands,
                                  float beat_strength) {
    // This animation is static, so no update logic needed here.
}

void StaticCircleAnimation::render(notcurses* nc) {
    if (!plane_) return;

    ncplane_erase(plane_);

    // Draw a static ASCII circle
    const unsigned int center_y = plane_rows_ / 2;
    const unsigned int center_x = plane_cols_ / 2;
    const unsigned int radius_y = std::min(plane_rows_, plane_cols_ / 2) / 2; // Adjust for aspect ratio
    const unsigned int radius_x = radius_y * 2; // Make it look circular in terminal

    for (unsigned int y = 0; y < plane_rows_; ++y) {
        for (unsigned int x = 0; x < plane_cols_; ++x) {
            // Check if point (x,y) is within the ellipse (circle)
            // (x-center_x)^2 / radius_x^2 + (y-center_y)^2 / radius_y^2 <= 1
            float dx = static_cast<float>(x) - center_x;
            float dy = static_cast<float>(y) - center_y;

            if ((dx * dx) / (radius_x * radius_x) + (dy * dy) / (radius_y * radius_y) <= 1.0f) {
                ncplane_set_fg_rgb8(plane_, 255, 255, 0); // Yellow
                ncplane_set_bg_rgb8(plane_, 0, 0, 0);   // Black background
                ncplane_putstr_yx(plane_, y, x, "#");
            }
        }
    }
}

} // namespace animations
} // namespace why