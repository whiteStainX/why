#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "audio_engine.h"

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
               bool show_overlay_metrics);


} // namespace why

