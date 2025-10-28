#include "renderer.h"

#include <string>
#include <random>
#include <chrono>

#include "animations/random_text_animation.h"

namespace why {

namespace {
static std::unique_ptr<animations::Animation> current_animation;
} // namespace

void set_active_animation(std::unique_ptr<animations::Animation> animation) {
    current_animation = std::move(animation);
}

void render_frame(notcurses* nc,
               float time_s,
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

    if (current_animation) {
        current_animation->render(nc, time_s, metrics, bands, beat_strength);
    }

    // Display overlay metrics if requested
    if (show_overlay_metrics && show_metrics) {
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200); // White foreground
        ncplane_set_bg_rgb8(stdplane, 0, 0, 0);     // Black background
        ncplane_printf_yx(stdplane, plane_rows - 3, 0,
                          "Audio %s",
                          metrics.active ? (file_stream ? "file" : "capturing") : "inactive"
                        );

        ncplane_printf_yx(stdplane, plane_rows - 2, 0,
                          "RMS: %.3f | Peak: %.3f | Dropped: %zu | Beat: %.2f",
                          metrics.rms,
                          metrics.peak,
                          metrics.dropped,
                          beat_strength);
    }
}

} // namespace why
