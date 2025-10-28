#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace why {
namespace {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct CellState {
    float smooth_r{0.0f};
    float smooth_g{0.0f};
    float smooth_b{0.0f};
    Rgb color{0, 0, 0};
    char glyph{' '};
    bool valid{false};
};

struct GridCache {
    int rows{0};
    int cols{0};
    int cell_h{0};
    int cell_w{0};
    int offset_y{0};
    int offset_x{0};
    std::vector<CellState> cells;
    std::string fill;
};

GridCache& grid_cache() {
    static GridCache cache;
    return cache;
}

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

float hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) {
        t += 1.0f;
    }
    if (t > 1.0f) {
        t -= 1.0f;
    }
    if (t < 1.0f / 6.0f) {
        return p + (q - p) * 6.0f * t;
    }
    if (t < 1.0f / 2.0f) {
        return q;
    }
    if (t < 2.0f / 3.0f) {
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    }
    return p;
}

Rgb hsl_to_rgb(float h, float s, float l) {
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) {
        h += 1.0f;
    }
    s = clamp01(s);
    l = clamp01(l);

    float r;
    float g;
    float b;

    if (s == 0.0f) {
        r = g = b = l;
    } else {
        const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        const float p = 2.0f * l - q;
        r = hue_to_rgb(p, q, h + 1.0f / 3.0f);
        g = hue_to_rgb(p, q, h);
        b = hue_to_rgb(p, q, h - 1.0f / 3.0f);
    }

    return Rgb{static_cast<uint8_t>(std::round(r * 255.0f)),
               static_cast<uint8_t>(std::round(g * 255.0f)),
               static_cast<uint8_t>(std::round(b * 255.0f))};
}

constexpr std::string_view kAsciiGlyphs =
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

std::string format_band_meter(const std::vector<float>& bands) {
    static const std::string glyphs = " .:-=+*#%@";
    if (bands.empty()) {
        return "Bands (unavailable)";
    }

    std::string line = "Bands ";
    for (float energy : bands) {
        const float scaled = std::log10(1.0f + std::max(energy, 0.0f) * 9.0f) / std::log10(10.0f);
        const float normalized = clamp01(scaled);
        const float position = normalized * static_cast<float>(glyphs.size() - 1);
        const int idx = static_cast<int>(std::round(position));
        line.push_back(glyphs[idx]);
    }
    return line;
}

} // namespace

const char* mode_name(VisualizationMode mode) {
    switch (mode) {
    case VisualizationMode::Bands:
        return "Bands";
    case VisualizationMode::Radial:
        return "Radial";
    case VisualizationMode::Trails:
        return "Trails";
    case VisualizationMode::Digital:
        return "Digital Pulse";
    case VisualizationMode::Ascii:
        return "ASCII Flux";
    default:
        return "Unknown";
    }
}

const char* palette_name(ColorPalette palette) {
    switch (palette) {
    case ColorPalette::Rainbow:
        return "Rainbow";
    case ColorPalette::WarmCool:
        return "Warm/Cool";
    case ColorPalette::DigitalAmber:
        return "Digital Amber";
    case ColorPalette::DigitalCyan:
        return "Digital Cyan";
    case ColorPalette::DigitalViolet:
        return "Digital Violet";
    default:
        return "Unknown";
    }
}

void draw_grid(notcurses* nc,
               int grid_rows,
               int grid_cols,
               float time_s,
               VisualizationMode mode,
               ColorPalette palette,
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

    const bool ascii_mode = mode == VisualizationMode::Ascii;

    const int cell_h_from_rows = grid_rows > 0 ? static_cast<int>(plane_rows) / grid_rows : 0;
    const int cell_h_from_cols = ascii_mode ? (grid_cols > 0 ? static_cast<int>(plane_cols) / grid_cols : 0)
                                            : (grid_cols > 0 ? static_cast<int>(plane_cols) / (grid_cols * 2) : 0);
    const int cell_h = std::max(1, std::min(cell_h_from_rows, cell_h_from_cols));
    const int cell_w = ascii_mode ? std::max(1, grid_cols > 0 ? static_cast<int>(plane_cols) / grid_cols : 1)
                                  : cell_h * 2;

    const int grid_height = cell_h * grid_rows;
    const int grid_width = cell_w * grid_cols;

    const int offset_y = std::max(0, (static_cast<int>(plane_rows) - grid_height) / 2);
    const int offset_x = std::max(0, (static_cast<int>(plane_cols) - grid_width) / 2);

    GridCache& cache = grid_cache();
    const bool geometry_changed = cache.rows != grid_rows || cache.cols != grid_cols || cache.cell_h != cell_h ||
                                  cache.cell_w != cell_w || cache.offset_y != offset_y || cache.offset_x != offset_x;

    if (geometry_changed) {
        ncplane_erase(stdplane);
        cache.rows = grid_rows;
        cache.cols = grid_cols;
        cache.cell_h = cell_h;
        cache.cell_w = cell_w;
        cache.offset_y = offset_y;
        cache.offset_x = offset_x;
        cache.cells.assign(static_cast<std::size_t>(grid_rows * grid_cols), CellState{});
    } else if (cache.cells.size() != static_cast<std::size_t>(grid_rows * grid_cols)) {
        cache.cells.assign(static_cast<std::size_t>(grid_rows * grid_cols), CellState{});
    }

    ncplane_set_fg_default(stdplane);

    const int v_gap = ascii_mode ? 0 : 1;
    const int h_gap = ascii_mode ? 0 : 2;
    const int fill_w = std::max(1, cell_w - h_gap);
    if (static_cast<int>(cache.fill.size()) != fill_w) {
        cache.fill.assign(static_cast<std::size_t>(fill_w), ' ');
    }
    const std::string& cell_fill = cache.fill;
    const int draw_height = std::max(1, cell_h - v_gap);

    const std::size_t band_count = bands.size();
    float max_band_energy = 0.0f;
    float mean_band_energy = 0.0f;
    if (band_count > 0) {
        for (float energy : bands) {
            max_band_energy = std::max(max_band_energy, energy);
            mean_band_energy += energy;
        }
        mean_band_energy /= static_cast<float>(band_count);
    }

    const float reference_energy = std::max(max_band_energy, mean_band_energy * 1.5f);
    const float user_gain = std::max(0.1f, sensitivity);
    const float gain = reference_energy > 0.0f ? user_gain / reference_energy : user_gain;
    const float log_denom = std::log1p(9.0f);

    auto normalize_energy = [&](float energy) {
        const float scaled = std::log1p(std::max(energy, 0.0f) * gain * 9.0f);
        return clamp01(log_denom > 0.0f ? scaled / log_denom : 0.0f);
    };

    const float center_row = (grid_rows - 1) / 2.0f;
    const float center_col = (grid_cols - 1) / 2.0f;
    const float max_radius = std::max(1.0f, std::sqrt(center_row * center_row + center_col * center_col));
    constexpr float inv_two_pi = 0.15915494309189535f; // 1 / (2π)

    const bool full_refresh = geometry_changed;

    const bool digital_mode = mode == VisualizationMode::Digital;
    const bool digital_palette = palette == ColorPalette::DigitalAmber || palette == ColorPalette::DigitalCyan ||
                                 palette == ColorPalette::DigitalViolet;
    const bool use_digital = (digital_mode || digital_palette) && !ascii_mode;

    const float beat_flash = clamp01(beat_strength);

    for (int r = 0; r < grid_rows; ++r) {
        for (int c = 0; c < grid_cols; ++c) {
            std::size_t band_index = 0;
            float band_mix = 0.0f;
            if (band_count > 0) {
                switch (mode) {
                case VisualizationMode::Bands:
                case VisualizationMode::Digital:
                case VisualizationMode::Ascii: {
                    const float band_t = static_cast<float>(r) / static_cast<float>(grid_rows);
                    band_index = std::min<std::size_t>(band_count - 1,
                                                        static_cast<std::size_t>(band_t * static_cast<float>(band_count)));
                    band_mix = static_cast<float>(band_index) / std::max<std::size_t>(1, band_count - 1);
                    break;
                }
                case VisualizationMode::Radial: {
                    const float dr = static_cast<float>(r) - center_row;
                    const float dc = static_cast<float>(c) - center_col;
                    const float radius = std::sqrt(dr * dr + dc * dc);
                    const float normalized = clamp01(radius / max_radius);
                    band_index = std::min<std::size_t>(band_count - 1,
                                                        static_cast<std::size_t>(normalized * static_cast<float>(band_count)));
                    band_mix = normalized;
                    break;
                }
                case VisualizationMode::Trails: {
                    const float column_phase = static_cast<float>(c) / std::max(1, grid_cols - 1);
                    float trail_phase = std::fmod(time_s * 0.35f + column_phase, 1.0f);
                    if (trail_phase < 0.0f) {
                        trail_phase += 1.0f;
                    }
                    band_index = std::min<std::size_t>(band_count - 1,
                                                        static_cast<std::size_t>(trail_phase * static_cast<float>(band_count)));
                    band_mix = trail_phase;
                    break;
                }
                }
            }

            const float band_energy = (band_index < band_count) ? bands[band_index] : 0.0f;
            const float energy_level = normalize_energy(band_energy);

            const float column_ratio = grid_cols > 1 ? static_cast<float>(c) / static_cast<float>(grid_cols - 1) : 0.0f;
            const float time_wave = use_digital ? 0.0f : std::sin(time_s * 1.3f + column_ratio * 3.0f);
            const float shimmer = use_digital ? 0.0f : std::sin(time_s * 0.9f + r * 0.35f + c * 0.22f);

            float target_r = 0.0f;
            float target_g = 0.0f;
            float target_b = 0.0f;
            float ascii_drive = 0.0f;
            char target_glyph = ' ';

            if (ascii_mode) {
                float high_energy = 0.0f;
                if (band_count > 0) {
                    const std::size_t lookahead = std::min<std::size_t>(band_count - 1,
                                                                         band_index + std::max<std::size_t>(1, band_count / 6));
                    high_energy = normalize_energy(bands[lookahead]);
                }
                const float jitter_a = std::sin(time_s * 2.6f + static_cast<float>(r) * 0.9f);
                const float jitter_b = std::cos(time_s * 1.8f + static_cast<float>(c) * 0.7f);
                const float jitter = (jitter_a + jitter_b) * 0.25f + 0.5f;
                ascii_drive = clamp01(0.45f * energy_level + 0.25f * high_energy + 0.15f * jitter + 0.15f * band_mix);
                ascii_drive = clamp01(ascii_drive + beat_strength * 0.25f);
            }

            if (use_digital) {
                Rgb base_color{200, 200, 200};
                int quantization_levels = 6;
                float design_multiplier = 1.0f;
                float beat_floor = 0.0f;
                float beat_color_shift = 0.0f; // New: for subtle color shifts on beat

                switch (palette) {
                case ColorPalette::DigitalAmber:
                    base_color = Rgb{255, 180, 48};
                    quantization_levels = 5;
                    if (beat_strength > 0.6f) {
                        beat_floor = 0.85f;
                        beat_color_shift = 0.1f; // Shift towards yellow/white
                    }
                    break;
                case ColorPalette::DigitalCyan: {
                    base_color = Rgb{48, 220, 255};
                    quantization_levels = 6;
                    const int scan_rate = 4;
                    if (grid_cols > 0) {
                        const int step = static_cast<int>(time_s * static_cast<float>(scan_rate));
                        int scan_col = 0;
                        if (grid_cols > 0) {
                            scan_col = step % grid_cols;
                            if (scan_col < 0) {
                                scan_col += grid_cols;
                            }
                        }
                        const int distance = std::abs(c - scan_col);
                        if (distance == 0) {
                            design_multiplier = 1.35f + beat_strength * 0.5f; // Brighter on beat
                        } else if (distance == 1) {
                            design_multiplier = 1.1f + beat_strength * 0.2f;
                        } else {
                            design_multiplier = 0.85f;
                        }
                    }
                    break;
                }
                case ColorPalette::DigitalViolet: {
                    base_color = Rgb{208, 64, 255};
                    quantization_levels = 7;
                    const int toggle = static_cast<int>(time_s * 8.0f) % 2;
                    const int parity = (r + c + toggle) % 2;
                    design_multiplier = (parity == 0) ? (1.2f + beat_strength * 0.3f) : (0.6f + beat_strength * 0.1f); // Pulse with beat
                    break;
                }
                default:
                    break;
                }

                const float base_r = static_cast<float>(base_color.r) / 255.0f;
                const float base_g = static_cast<float>(base_color.g) / 255.0f;
                const float base_b = static_cast<float>(base_color.b) / 255.0f;

                // More aggressive beat reaction and non-linear energy mapping
                const float beat_multiplier = 1.0f + clamp01(beat_strength) * 1.2f; // Increased multiplier
                float intensity = std::pow(energy_level, 1.5f) * beat_multiplier * design_multiplier; // Non-linear energy

                if (beat_floor > 0.0f) {
                    intensity = std::max(intensity, beat_floor);
                }

                // Dynamic quantization
                int current_quantization_levels = quantization_levels;
                if (beat_strength > 0.5f) {
                    current_quantization_levels = std::max(2, quantization_levels - 2); // Fewer levels on strong beats
                }
                if (current_quantization_levels > 1) {
                    intensity = std::round(intensity * static_cast<float>(current_quantization_levels)) /
                                static_cast<float>(current_quantization_levels);
                }
                if (intensity < 0.05f) {
                    intensity = 0.0f;
                }

                target_r = clamp01(base_r * intensity + beat_color_shift);
                target_g = clamp01(base_g * intensity + beat_color_shift);
                target_b = clamp01(base_b * intensity + beat_color_shift);
            } else {
                float base_hue = column_ratio;
                if (band_count > 0) {
                    switch (mode) {
                    case VisualizationMode::Bands:
                        base_hue = static_cast<float>(band_index) / static_cast<float>(band_count);
                        break;
                    case VisualizationMode::Radial: {
                        const float dr = static_cast<float>(r) - center_row;
                        const float dc = static_cast<float>(c) - center_col;
                        const float angle = std::atan2(dr, dc);
                        base_hue = std::fmod(angle * inv_two_pi + 1.0f, 1.0f);
                        break;
                    }
                    case VisualizationMode::Trails:
                        base_hue = band_mix;
                        break;
                    case VisualizationMode::Digital: // Should not be reached due to use_digital check
                    case VisualizationMode::Ascii:    // Should not be reached due to ascii_mode check
                        base_hue = static_cast<float>(band_index) / std::max<std::size_t>(1, band_count);
                        break;
                    }
                }

                const float hue_shift = std::fmod(time_s * 0.05f + column_ratio * 0.15f, 1.0f);

                float hue = std::fmod(base_hue + hue_shift, 1.0f);
                float saturation = clamp01(0.55f + energy_level * 0.4f + shimmer * 0.05f);
                float brightness = clamp01(0.12f + energy_level * (0.82f + beat_flash * 0.35f) + time_wave * 0.12f +
                                           beat_flash * 0.12f);

                if (palette == ColorPalette::WarmCool) {
                    const float warm_cool_base = clamp01(band_mix);
                    const float warm_cool_hue = std::fmod(0.58f - warm_cool_base * 0.42f + shimmer * 0.02f, 1.0f);
                    hue = std::fmod(warm_cool_hue + beat_flash * 0.05f, 1.0f);
                    saturation = clamp01(0.45f + energy_level * 0.35f + shimmer * 0.08f);
                    brightness = clamp01(0.18f + energy_level * (0.75f + beat_flash * 0.45f) + time_wave * 0.08f +
                                          beat_flash * 0.18f);
                }

                if (ascii_mode) {
                    saturation = clamp01(saturation + 0.2f + ascii_drive * 0.1f);
                    brightness = clamp01(brightness + ascii_drive * 0.4f + beat_flash * 0.1f);
                }

                const Rgb target_color = hsl_to_rgb(hue, saturation, brightness);
                target_r = clamp01(static_cast<float>(target_color.r) / 255.0f);
                target_g = clamp01(static_cast<float>(target_color.g) / 255.0f);
                target_b = clamp01(static_cast<float>(target_color.b) / 255.0f);
            }

            const std::size_t cell_index = static_cast<std::size_t>(r * grid_cols + c);
            if (cell_index >= cache.cells.size()) {
                continue;
            }

            CellState& state = cache.cells[cell_index];
            if (!state.valid || full_refresh) {
                state.smooth_r = target_r;
                state.smooth_g = target_g;
                state.smooth_b = target_b;
            } else {
                // Adjust smoothing based on beat_strength for more reactivity
                float base_smoothing = 0.22f;
                if (use_digital) {
                    base_smoothing = 0.55f;
                } else if (ascii_mode) {
                    base_smoothing = 0.3f;
                }
                const float dynamic_smoothing = base_smoothing + beat_strength * 0.3f; // More reactive on beat
                state.smooth_r += (target_r - state.smooth_r) * dynamic_smoothing;
                state.smooth_g += (target_g - state.smooth_g) * dynamic_smoothing;
                state.smooth_b += (target_b - state.smooth_b) * dynamic_smoothing;
            }

            if (ascii_mode) {
                const float luma = clamp01(state.smooth_r * 0.299f + state.smooth_g * 0.587f + state.smooth_b * 0.114f);
                const float swirl = (std::sin(time_s * 6.2f + static_cast<float>(r) * 0.8f - static_cast<float>(c) * 0.9f) + 1.0f) *
                                    0.5f;
                const float texture = (std::sin(time_s * 3.3f + band_mix * 6.2831853f + static_cast<float>(r + c) * 0.35f) + 1.0f) *
                                      0.5f;
                const float ascii_mix = clamp01(0.45f * ascii_drive + 0.35f * luma + 0.2f * swirl);
                const float ascii_value = clamp01(ascii_mix * 0.75f + texture * 0.25f + beat_strength * 0.2f);
                const std::size_t glyph_count = kAsciiGlyphs.size();
                const std::size_t glyph_idx = (glyph_count > 1)
                                                  ? std::min<std::size_t>(
                                                        glyph_count - 1,
                                                        static_cast<std::size_t>(std::round(
                                                            ascii_value * static_cast<float>(glyph_count - 1))))
                                                  : 0;
                target_glyph = static_cast<char>(kAsciiGlyphs[glyph_idx]);
            }

            const Rgb color{static_cast<uint8_t>(std::round(clamp01(state.smooth_r) * 255.0f)),
                            static_cast<uint8_t>(std::round(clamp01(state.smooth_g) * 255.0f)),
                            static_cast<uint8_t>(std::round(clamp01(state.smooth_b) * 255.0f))};

            bool needs_update = full_refresh || !state.valid || state.color.r != color.r || state.color.g != color.g ||
                                state.color.b != color.b;

            if (ascii_mode && state.glyph != target_glyph) {
                needs_update = true;
            }

            if (!needs_update) {
                continue;
            }

            if (ascii_mode) {
                const std::string ascii_fill(static_cast<std::size_t>(fill_w), target_glyph);
                for (int dy = 0; dy < draw_height; ++dy) {
                    const int y = offset_y + r * cell_h + dy;
                    if (y >= static_cast<int>(plane_rows)) {
                        continue;
                    }
                    const int x = offset_x + c * cell_w;
                    if (x >= static_cast<int>(plane_cols)) {
                        continue;
                    }

                    ncplane_set_bg_default(stdplane);
                    ncplane_set_fg_rgb8(stdplane, color.r, color.g, color.b);
                    ncplane_putstr_yx(stdplane, y, x, ascii_fill.c_str());
                }
            } else {
                for (int dy = 0; dy < draw_height; ++dy) {
                    const int y = offset_y + r * cell_h + dy;
                    if (y >= static_cast<int>(plane_rows)) {
                        continue;
                    }
                    const int x = offset_x + c * cell_w;
                    if (x >= static_cast<int>(plane_cols)) {
                        continue;
                    }

                    ncplane_set_bg_rgb8(stdplane, color.r, color.g, color.b);
                    ncplane_putstr_yx(stdplane, y, x, cell_fill.c_str());
                }
            }

            state.color = color;
            state.glyph = target_glyph;
            state.valid = true;
        }
    }

    const int overlay_y = std::min(static_cast<int>(plane_rows) - 1, offset_y + grid_height);
    const int overlay_x = offset_x;
    auto clear_overlay_line = [&](int y) {
        if (y >= static_cast<int>(plane_rows)) {
            return;
        }
        const int width = std::max(0, static_cast<int>(plane_cols) - overlay_x);
        if (width <= 0) {
            return;
        }
        ncplane_set_fg_default(stdplane);
        ncplane_set_bg_default(stdplane);
        ncplane_printf_yx(stdplane, y, overlay_x, "%*s", width, "");
    };

    if (!show_overlay_metrics) {
        clear_overlay_line(overlay_y);
        clear_overlay_line(overlay_y + 1);
        clear_overlay_line(overlay_y + 2);
        return;
    }

    if (!show_metrics) {
        clear_overlay_line(overlay_y);
        clear_overlay_line(overlay_y + 1);
        clear_overlay_line(overlay_y + 2);
        return;
    }

    clear_overlay_line(overlay_y);
    ncplane_set_fg_rgb8(stdplane, 200, 200, 200);
    ncplane_set_bg_default(stdplane);
    ncplane_printf_yx(stdplane, overlay_y, overlay_x,
                      "Audio %s | Mode: %s | Palette: %s | Grid: %dx%d | Sens: %.2f",
                      metrics.active ? (file_stream ? "file" : "capturing") : "inactive",
                      mode_name(mode),
                      palette_name(palette),
                      grid_rows,
                      grid_cols,
                      sensitivity);

    if (overlay_y + 1 < static_cast<int>(plane_rows)) {
        clear_overlay_line(overlay_y + 1);
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200);
        ncplane_set_bg_default(stdplane);
        ncplane_printf_yx(stdplane, overlay_y + 1, overlay_x,
                          "RMS: %.3f | Peak: %.3f | Dropped: %zu | Beat: %.2f",
                          metrics.rms,
                          metrics.peak,
                          metrics.dropped,
                          beat_flash);
    }

    if (overlay_y + 2 < static_cast<int>(plane_rows)) {
        clear_overlay_line(overlay_y + 2);
        ncplane_set_fg_rgb8(stdplane, 200, 200, 200);
        ncplane_set_bg_default(stdplane);
        const std::string band_meter = format_band_meter(bands);
        ncplane_printf_yx(stdplane, overlay_y + 2, overlay_x, "%s", band_meter.c_str());
    }
}

} // namespace why

