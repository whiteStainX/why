#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "../audio_engine.h"
#include "../config.h" // Include AppConfig

namespace why {
namespace animations {

class Animation {
public:
    virtual ~Animation() = default;

    // Lifecycle methods
    virtual void init(notcurses* nc, const AppConfig& config) = 0;
    virtual void update(float delta_time,
                        const AudioMetrics& metrics,
                        const std::vector<float>& bands,
                        float beat_strength) = 0;
    virtual void render(notcurses* nc) = 0;

    // State queries
    virtual bool is_active() const = 0;
    virtual int get_z_index() const = 0;
    virtual ncplane* get_plane() const = 0; // Or a way to get the primary plane
};

} // namespace animations
} // namespace why