#include "renderer.h"

#include <string>
#include <random>
#include <chrono>

namespace why {

// These functions are kept as they might be used by other parts of the application
// or for future expansion, even if not directly used in the simplified draw_grid.




void draw_grid(notcurses* nc,
               int grid_rows,
               int grid_cols,
               float time_s,
               float sensitivity,
               const AudioMetrics& metrics,
               const std::vector<float>& bands,
               float beat_strength,
               bool file_stream,
               bool show_metrics,
               bool show_overlay_metrics) {
    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(stdplane, &plane_rows, &plane_cols);

    // Clear the screen
    ncplane_erase(stdplane);

    // Seed the random number generator
    static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=[]{}|;:',.<>/?";
    static std::uniform_int_distribution<std::string::size_type> dist(0, chars.size() - 1);

    // Display random text
    std::string random_text;
    for (unsigned int i = 0; i < plane_cols / 2; ++i) { // Display half the width to avoid overflow with wide characters
        random_text += chars[dist(rng)];
    }

    ncplane_set_fg_rgb8(stdplane, 255, 255, 255); // White foreground
    ncplane_set_bg_rgb8(stdplane, 0, 0, 0);     // Black background
    ncplane_putstr_yx(stdplane, plane_rows / 2, (plane_cols - random_text.length()) / 2, random_text.c_str());

    // Display overlay metrics if requested
    if (show_overlay_metrics && show_metrics) {
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200);
        ncplane_set_bg_default(stdplane);
        ncplane_printf_yx(stdplane, plane_rows - 3, 0,
                          "Audio %s | Grid: %dx%d | Sens: %.2f",
                          metrics.active ? (file_stream ? "file" : "capturing") : "inactive",
                          grid_rows,
                          grid_cols,
                          sensitivity);

        ncplane_printf_yx(stdplane, plane_rows - 2, 0,
                          "RMS: %.3f | Peak: %.3f | Dropped: %zu | Beat: %.2f",
                          metrics.rms,
                          metrics.peak,
                          metrics.dropped,
                          beat_strength);
    }
}

} // namespace why
