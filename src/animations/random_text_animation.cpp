#include "random_text_animation.h"

namespace why {
namespace animations {

const std::string RandomTextAnimation::chars_ =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=[]{}|;:',.<>/?";

RandomTextAnimation::RandomTextAnimation()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist_(0, chars_.size() - 1) {}

void RandomTextAnimation::render(notcurses* nc,
                                 int grid_rows,
                                 int grid_cols,
                                 float time_s,
                                 float sensitivity,
                                 const AudioMetrics& metrics,
                                 const std::vector<float>& bands,
                                 float beat_strength) {
    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(stdplane, &plane_rows, &plane_cols);

    // Clear the screen
    ncplane_erase(stdplane);

    // Display random text
    std::string random_text;
    for (unsigned int i = 0; i < plane_cols / 2; ++i) { // Display half the width to avoid overflow with wide characters
        random_text += chars_[dist_(rng_)];
    }

    ncplane_set_fg_rgb8(stdplane, 255, 255, 255); // White foreground
    ncplane_set_bg_rgb8(stdplane, 0, 0, 0);     // Black background
    ncplane_putstr_yx(stdplane, plane_rows / 2, (plane_cols - random_text.length()) / 2, random_text.c_str());
}

} // namespace animations
} // namespace why