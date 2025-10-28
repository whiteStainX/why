#include "animation_manager.h"
#include <iostream> // For std::clog and std::cerr
#include "random_text_animation.h" // Include for RandomTextAnimation
#include "bar_visual_animation.h" // Include for BarVisualAnimation
#include "static_circle_animation.h" // Include for StaticCircleAnimation

namespace why {
namespace animations {

namespace {
std::string clean_string_value(std::string value) {
    // Trim leading whitespace
    size_t first = value.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return "";
    }
    // Trim trailing whitespace
    size_t last = value.find_last_not_of(" \t\n\r\f\v");
    value = value.substr(first, (last - first + 1));

    // Remove surrounding quotes if present
    if (value.length() >= 2 &&
        ((value.front() == '\"' && value.back() == '\"' ) ||
         (value.front() == '\'' && value.back() == '\'' ))) {
        value = value.substr(1, value.length() - 2);
    }
    return value;
}
} // namespace

void AnimationManager::add_animation(std::unique_ptr<Animation> animation) {
    animations_.push_back(std::move(animation));
}

void AnimationManager::load_animations(notcurses* nc, const AppConfig& config) {
    std::clog << "[AnimationManager::load_animations] Attempting to load " << config.animations.size() << " animations." << std::endl;
    for (const auto& anim_config : config.animations) {
        std::clog << "[AnimationManager::load_animations] Processing animation config: type=" << anim_config.type << ", z_index=" << anim_config.z_index << std::endl;
        std::unique_ptr<Animation> new_animation;
        std::string cleaned_type = clean_string_value(anim_config.type);
        if (cleaned_type == "RandomText") {
            new_animation = std::make_unique<RandomTextAnimation>();
            std::clog << "[AnimationManager::load_animations] Created RandomTextAnimation." << std::endl;
        } else if (cleaned_type == "BarVisual") {
            new_animation = std::make_unique<BarVisualAnimation>();
            std::clog << "[AnimationManager::load_animations] Created BarVisualAnimation." << std::endl;
        } else if (cleaned_type == "StaticCircle") {
            new_animation = std::make_unique<StaticCircleAnimation>();
            std::clog << "[AnimationManager::load_animations] Created StaticCircleAnimation." << std::endl;
        }
        // Add more animation types here as they are implemented

        if (new_animation) {
            new_animation->init(nc, config);
            animations_.push_back(std::move(new_animation));
            std::clog << "[AnimationManager::load_animations] Initialized and added animation." << std::endl;
        } else {
            std::cerr << "[AnimationManager::load_animations] Unknown animation type: " << anim_config.type << std::endl;
        }
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

    // Explicitly set Z-order for each plane based on sorted order (lowest first)
    for (const auto& anim : animations_) {
        if (anim->get_plane()) {
            ncplane_move_top(anim->get_plane());
        }
    }

    for (const auto& anim : animations_) {
        if (anim->is_active()) {
            anim->render(nc);
        }
    }
}

} // namespace animations
} // namespace why