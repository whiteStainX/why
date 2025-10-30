#include "animation_manager.h"

#include <iostream>

#include "random_text_animation.h"
#include "bar_visual_animation.h"
#include "ascii_matrix_animation.h"
#include "cyber_rain_animation.h"
#include "lightning_wave_animation.h"
#include "breathe_animation.h"
#include "logging_animation.h"

namespace why {
namespace animations {

namespace {
std::string clean_string_value(std::string value) {
    size_t first = value.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return "";
    }

    size_t last = value.find_last_not_of(" \t\n\r\f\v");
    value = value.substr(first, (last - first + 1));

    if (value.length() >= 2 &&
        ((value.front() == '\"' && value.back() == '\"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.length() - 2);
    }
    return value;
}

bool has_custom_triggers(const AnimationConfig& config) {
    return config.trigger_band_index != -1 ||
           config.trigger_beat_min > 0.0f ||
           config.trigger_beat_max < 1.0f;
}

bool evaluate_band_condition(const AnimationConfig& config,
                             const std::vector<float>& bands) {
    if (config.trigger_band_index == -1) {
        return true;
    }

    const int index = config.trigger_band_index;
    if (index < 0 || index >= static_cast<int>(bands.size())) {
        return false;
    }

    return bands[static_cast<std::size_t>(index)] >= config.trigger_threshold;
}

bool evaluate_beat_condition(const AnimationConfig& config, float beat_strength) {
    if (config.trigger_beat_min <= 0.0f && config.trigger_beat_max >= 1.0f) {
        return true;
    }

    return beat_strength >= config.trigger_beat_min &&
           beat_strength <= config.trigger_beat_max;
}

} // namespace

void AnimationManager::load_animations(notcurses* nc, const AppConfig& app_config) {
    event_bus_.reset();
    animations_.clear();
    animations_.reserve(app_config.animations.size());

    for (const auto& anim_config : app_config.animations) {
        std::unique_ptr<Animation> new_animation;
        std::string cleaned_type = clean_string_value(anim_config.type);

        if (cleaned_type == "RandomText") {
            new_animation = std::make_unique<RandomTextAnimation>();
        } else if (cleaned_type == "BarVisual") {
            new_animation = std::make_unique<BarVisualAnimation>();
        } else if (cleaned_type == "AsciiMatrix") {
            new_animation = std::make_unique<AsciiMatrixAnimation>();
        } else if (cleaned_type == "CyberRain") {
            new_animation = std::make_unique<CyberRainAnimation>();
        } else if (cleaned_type == "LightningWave") {
            new_animation = std::make_unique<LightningWaveAnimation>();
        } else if (cleaned_type == "Breathe") {
            new_animation = std::make_unique<BreatheAnimation>();
        } else if (cleaned_type == "Logging") {
            new_animation = std::make_unique<LoggingAnimation>();
        }

        if (new_animation) {
            new_animation->init(nc, app_config);

            auto managed = std::make_unique<ManagedAnimation>();
            managed->config = anim_config;
            managed->animation = std::move(new_animation);

            managed->animation->bind_events(managed->config, event_bus_);
            register_animation_callbacks(*managed);
            animations_.push_back(std::move(managed));
        } else {
            // std::cerr << "[AnimationManager::load_animations] Unknown animation type: " << anim_config.type << std::endl;
        }
    }
}

void AnimationManager::register_animation_callbacks(ManagedAnimation& managed_animation) {
    if (!managed_animation.animation) {
        return;
    }

    ManagedAnimation* managed_ptr = &managed_animation;
    event_bus_.subscribe<events::FrameUpdateEvent>(
        [managed_ptr](const events::FrameUpdateEvent& event) {
            Animation* animation = managed_ptr->animation.get();
            const AnimationConfig& config = managed_ptr->config;

            bool meets_band = evaluate_band_condition(config, event.bands);
            bool meets_beat = evaluate_beat_condition(config, event.beat_strength);
            bool meets_all_conditions = meets_band && meets_beat;

            bool should_be_active = has_custom_triggers(config)
                                        ? meets_all_conditions
                                        : config.initially_active;

            if (should_be_active && !animation->is_active()) {
                animation->activate();
            } else if (!should_be_active && animation->is_active()) {
                animation->deactivate();
            }

            if (animation->is_active()) {
                animation->update(event.delta_time, event.metrics, event.bands, event.beat_strength);
            }
        });
}

void AnimationManager::update_all(float delta_time,
                                  const AudioMetrics& metrics,
                                  const std::vector<float>& bands,
                                  float beat_strength) {
    events::BeatDetectedEvent beat_event{beat_strength};
    event_bus_.publish(beat_event);

    events::FrameUpdateEvent frame_event{delta_time, metrics, bands, beat_strength};
    event_bus_.publish(frame_event);
}

void AnimationManager::render_all(notcurses* nc) {
    std::sort(animations_.begin(), animations_.end(), [](const auto& a, const auto& b) {
        return a->animation->get_z_index() < b->animation->get_z_index();
    });

    for (const auto& managed_anim : animations_) {
        if (auto* plane = managed_anim->animation->get_plane()) {
            ncplane_move_bottom(plane);
        }
    }

    for (const auto& managed_anim : animations_) {
        if (managed_anim->animation->is_active()) {
            managed_anim->animation->render(nc);
        }
    }
}

} // namespace animations
} // namespace why

