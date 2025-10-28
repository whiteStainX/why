#include "animation_manager.h"

namespace why {
namespace animations {

void AnimationManager::add_animation(std::unique_ptr<Animation> animation) {
    animations_.push_back(std::move(animation));
}

void AnimationManager::init_all(notcurses* nc, const AppConfig& config) {
    for (const auto& anim : animations_) {
        anim->init(nc, config);
    }
}

void AnimationManager::update_all(float delta_time,
                                  const AudioMetrics& metrics,
                                  const std::vector<float>& bands,
                                  float beat_strength) {
    for (const auto& anim : animations_) {
        if (anim->is_active()) {
            anim->update(delta_time, metrics, bands, beat_strength);
        }
    }
}

void AnimationManager::render_all(notcurses* nc) {
    // Sort animations by Z-index before rendering
    std::sort(animations_.begin(), animations_.end(), [](const auto& a, const auto& b) {
        return a->get_z_index() < b->get_z_index();
    });

    for (const auto& anim : animations_) {
        if (anim->is_active()) {
            anim->render(nc);
        }
    }
}

} // namespace animations
} // namespace why