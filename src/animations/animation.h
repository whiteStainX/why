#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "../audio_engine.h"

namespace why {
namespace animations {

class Animation {
public:
    virtual ~Animation() = default;
    virtual void render(notcurses* nc,
                        int grid_rows,
                        int grid_cols,
                        float time_s,
                        float sensitivity,
                        const AudioMetrics& metrics,
                        const std::vector<float>& bands,
                        float beat_strength) = 0;
};

} // namespace animations
} // namespace why