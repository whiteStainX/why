#include "renderer.h"

#include <string>
#include <random>
#include <chrono>

#include "animations/random_text_animation.h"

namespace why {

void render_frame(notcurses* nc,
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

    static animations::RandomTextAnimation random_text_animation;
    random_text_animation.render(nc, grid_rows, grid_cols, time_s, sensitivity, metrics, bands, beat_strength);

    // Display overlay metrics if requested
    if (show_overlay_metrics && show_metrics) {
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200); // White foreground
        ncplane_set_bg_rgb8(stdplane, 0, 0, 0);     // Black background
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
