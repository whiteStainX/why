#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "audio_engine.h"

#include "animations/animation.h"



namespace why {



void render_frame(notcurses* nc,



               float time_s,



               const AudioMetrics& metrics,



               const std::vector<float>& bands,



               float beat_strength,



               bool file_stream,



               bool show_metrics,



               bool show_overlay_metrics);



void set_active_animation(std::unique_ptr<animations::Animation> animation);



} // namespace why

