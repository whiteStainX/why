#pragma once

#include <vector>

#include <notcurses/notcurses.h>

#include "animation.h"
#include "../config.h"

namespace why {
namespace animations {

class StaticCircleAnimation : public Animation {
public:
    StaticCircleAnimation();
    ~StaticCircleAnimation() override;

    void init(notcurses* nc, const AppConfig& config) override;
    void update(float delta_time,
                const AudioMetrics& metrics,
                const std::vector<float>& bands,
                float beat_strength) override;
    void render(notcurses* nc) override;

    bool is_active() const override { return true; } // Always active for now
    int get_z_index() const override { return z_index_; }
    ncplane* get_plane() const override { return plane_; }

private:
    ncplane* plane_ = nullptr;
    int z_index_ = 0;
    unsigned int plane_rows_ = 0;
    unsigned int plane_cols_ = 0;
};

} // namespace animations
} // namespace why