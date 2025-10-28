#pragma once

#include <memory>
#include <vector>
#include <algorithm>

#include <notcurses/notcurses.h>

#include "animation.h"
#include "../config.h"

namespace why {
namespace animations {

class AnimationManager {
public:
    AnimationManager() = default;
    ~AnimationManager() = default;

    void add_animation(std::unique_ptr<Animation> animation);
    void init_all(notcurses* nc, const AppConfig& config);
    void update_all(float delta_time,
                    const AudioMetrics& metrics,
                    const std::vector<float>& bands,
                    float beat_strength);
    void render_all(notcurses* nc);

private:
    std::vector<std::unique_ptr<Animation>> animations_;
};

} // namespace animations
} // namespace why