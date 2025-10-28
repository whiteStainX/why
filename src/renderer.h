#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "audio_engine.h"

namespace why {



enum class ColorPalette {
    Rainbow,
    WarmCool,
    DigitalAmber,
    DigitalCyan,
    DigitalViolet,
};

void draw_grid(notcurses* nc,
               int grid_rows,
               int grid_cols,
               float time_s,
               ColorPalette palette,
               float sensitivity,
               const AudioMetrics& metrics,
               const std::vector<float>& bands,
               float beat_strength,
               bool file_stream,
               bool show_metrics,
               bool show_overlay_metrics);

const char* palette_name(ColorPalette palette);

} // namespace why

