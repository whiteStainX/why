#include "renderer.h"

#include <string>
#include <random>
#include <chrono>

#include "animations/random_text_animation.h"
#include "animations/animation_manager.h"

namespace why {

namespace {
static animations::AnimationManager animation_manager;
} // namespace

// set_active_animation is removed, AnimationManager handles adding animations
void add_animation_to_manager(std::unique_ptr<animations::Animation> animation) {
    animation_manager.add_animation(std::move(animation));
}

void init_animation_manager(notcurses* nc, const AppConfig& config) {
    animation_manager.init_all(nc, config);
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

    // Clear the standard plane (background)
    ncplane_erase(stdplane);

    // Update and render all animations managed by the AnimationManager
    animation_manager.update_all(time_s, metrics, bands, beat_strength);
    animation_manager.render_all(nc);

    // Display overlay metrics if requested
    if (show_overlay_metrics && show_metrics) {
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200); // White foreground
        ncplane_set_bg_rgb8(stdplane, 0, 0, 0);     // Black background
        ncplane_printf_yx(stdplane, plane_rows - 3, 0,
                          "Audio %s",
                          metrics.active ? (file_stream ? "file" : "capturing") : "inactive");

        ncplane_printf_yx(stdplane, plane_rows - 2, 0,
                          "RMS: %.3f | Peak: %.3f | Dropped: %zu | Beat: %.2f",
                          metrics.rms,
                          metrics.peak,
                          metrics.dropped,
                          beat_strength);
    }
}

} // namespace why
