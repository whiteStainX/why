#include "animation_manager.h"
#include <iostream> // For std::clog and std::cerr
#include "random_text_animation.h" // Include for RandomTextAnimation
#include "bar_visual_animation.h" // Include for BarVisualAnimation

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

void AnimationManager::load_animations(notcurses* nc, const AppConfig& app_config) {
    std::clog << "[AnimationManager::load_animations] Attempting to load " << app_config.animations.size() << " animations." << std::endl;
    for (const auto& anim_config : app_config.animations) {
        std::clog << "[AnimationManager::load_animations] Processing animation config: type=" << anim_config.type << ", z_index=" << anim_config.z_index << ", initially_active=" << anim_config.initially_active << std::endl;
        std::unique_ptr<Animation> new_animation;
        std::string cleaned_type = clean_string_value(anim_config.type);

        if (cleaned_type == "RandomText") {
            new_animation = std::make_unique<RandomTextAnimation>();
            std::clog << "[AnimationManager::load_animations] Created RandomTextAnimation." << std::endl;
        } else if (cleaned_type == "BarVisual") {
            new_animation = std::make_unique<BarVisualAnimation>();
            std::clog << "[AnimationManager::load_animations] Created BarVisualAnimation." << std::endl;
        }
        // Add more animation types here as they are implemented

        if (new_animation) {
            new_animation->init(nc, app_config); // Pass the full app_config for init
            animations_.push_back({std::move(new_animation), anim_config});
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
    for (auto& managed_anim : animations_) {
        Animation* anim = managed_anim.animation.get();
        const AnimationConfig& config = managed_anim.config;

        bool should_be_active = config.initially_active; // Start with initial state

        // Evaluate trigger conditions
        if (config.trigger_band_index != -1 && config.trigger_band_index < static_cast<int>(bands.size())) {
            if (bands[config.trigger_band_index] < config.trigger_threshold) {
                should_be_active = false; // Deactivate if band energy is below threshold
            }
        }

        if (beat_strength < config.trigger_beat_min || beat_strength > config.trigger_beat_max) {
            should_be_active = false; // Deactivate if beat strength is outside range
        }

        // Apply activation/deactivation
        if (should_be_active && !anim->is_active()) {
            anim->activate();
        } else if (!should_be_active && anim->is_active()) {
            anim->deactivate();
        }

        // Update active animations
        if (anim->is_active()) {
            anim->update(delta_time, metrics, bands, beat_strength);
        }
    }
}

void AnimationManager::render_all(notcurses* nc) {
    // Sort animations by Z-index before rendering
    std::sort(animations_.begin(), animations_.end(), [](const auto& a, const auto& b) {
        return a.animation->get_z_index() < b.animation->get_z_index();
    });

    // Explicitly set Z-order for each plane
    for (const auto& managed_anim : animations_) {
        if (managed_anim.animation->get_plane()) {
            // Move planes to the bottom first, then they will be moved up by subsequent animations
            // based on their sorted z_index. This ensures correct relative ordering.
            ncplane_move_bottom(managed_anim.animation->get_plane());
        }
    }

    for (const auto& managed_anim : animations_) {
        if (managed_anim.animation->is_active()) {
            managed_anim.animation->render(nc);
        }
    }
}

} // namespace animations
} // namespace why