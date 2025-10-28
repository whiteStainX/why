#include <notcurses/notcurses.h>
#include <iostream> // For std::clog and std::cerr

#include "random_text_animation.h"

namespace why {
namespace animations {

const std::string RandomTextAnimation::chars_ =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=[]{}|;:',.<>/?";

RandomTextAnimation::RandomTextAnimation()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist_(0, chars_.size() - 1) {}

RandomTextAnimation::~RandomTextAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
    }
}

void RandomTextAnimation::init(notcurses* nc, const AppConfig& config) {
    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(stdplane, &plane_rows, &plane_cols);

    // Log dimensions for debugging
    std::clog << "[RandomTextAnimation::init] stdplane dimensions: " << plane_rows << "x" << plane_cols << std::endl;

    ncplane_options p_opts{};
    p_opts.rows = plane_rows;
    p_opts.cols = plane_cols;
    p_opts.y = 0;
    p_opts.x = 0;
    plane_ = ncplane_create(stdplane, &p_opts);
    if (!plane_) {
        std::cerr << "[RandomTextAnimation::init] Failed to create ncplane!" << std::endl;
    } else {
        std::clog << "[RandomTextAnimation::init] ncplane created successfully." << std::endl;
    }

    // Set z-index and initial active state from config
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "RandomText") { // Assuming type is used to identify
            z_index_ = anim_config.z_index;
            is_active_ = anim_config.initially_active;
            break;
        }
    }
}

void RandomTextAnimation::activate() {
    is_active_ = true;
    if (plane_) {
        ncplane_set_fg_rgb8(plane_, 255, 255, 255); // White foreground
        ncplane_set_bg_rgb8(plane_, 0, 0, 0);     // Black background
    }
}

void RandomTextAnimation::deactivate() {
    is_active_ = false;
    if (plane_) {
        ncplane_erase(plane_); // Clear the plane when deactivated
    }
}

void RandomTextAnimation::update(float delta_time,
                                 const AudioMetrics& metrics,
                                 const std::vector<float>& bands,
                                 float beat_strength) {
    if (!plane_ || !is_active_) return;

    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(plane_, &plane_rows, &plane_cols);

    current_text_.clear();
    for (unsigned int i = 0; i < plane_cols / 2; ++i) { // Display half the width to avoid overflow with wide characters
        current_text_ += chars_[dist_(rng_)];
    }
}

void RandomTextAnimation::render(notcurses* nc) {
    if (!plane_ || !is_active_) return;

    ncplane_erase(plane_);

    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(plane_, &plane_rows, &plane_cols);

    ncplane_set_fg_rgb8(plane_, 255, 255, 255); // White foreground
    ncplane_set_bg_rgb8(plane_, 0, 0, 0);     // Black background
    ncplane_putstr_yx(plane_, plane_rows / 2, (plane_cols - current_text_.length()) / 2, current_text_.c_str());
}

} // namespace animations
} // namespace why