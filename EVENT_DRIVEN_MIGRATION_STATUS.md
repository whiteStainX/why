# Event-Driven Architecture: Migration Status

This document tracks the progress of migrating the animation system to a fully event-driven architecture. It outlines what has been completed, analyzes the current state, and details the next steps required.

---

## Phase 1: Foundation (Completed)

The foundational infrastructure for an event-driven system has been successfully implemented. This marks a significant improvement in the codebase's modularity and decouples major components.

### What is Done:
*   **`EventBus` Created:** A robust, header-only `why::events::EventBus` now exists in `src/events/event_bus.h`. It allows for type-safe subscribing and publishing of events.
*   **Core Events Defined:** The initial event types, `FrameUpdateEvent` and `BeatDetectedEvent`, have been created to carry data from the core loops to potential subscribers.
*   **Decoupling of Emitters:** The `DspEngine` and the main loop in `main.cpp` no longer have direct knowledge of the animation system. They now correctly publish their data as events to the `EventBus`.
*   **`AnimationManager` as a Coordinator:** The `AnimationManager`'s role has been simplified. It now primarily coordinates the loading of animations and publishes the high-level `FrameUpdateEvent`.

---

## Phase 2: Analysis of Current State (In-Progress)

While the foundation is in place, the system currently operates in a **hybrid model**. An event bus is used for communication, but the core animation trigger logic still relies on polling.

### The "Hybrid" Polling Problem:

Currently, `AnimationManager::register_animation_callbacks` subscribes **every single animation** to the `FrameUpdateEvent`.

```cpp
// In AnimationManager.cpp
event_bus_.subscribe<events::FrameUpdateEvent>(
    [managed_ptr](const events::FrameUpdateEvent& event) {
        // ...
        // This logic is run for EVERY animation on EVERY frame:
        bool meets_band = evaluate_band_condition(config, event.bands);
        bool meets_beat = evaluate_beat_condition(config, event.beat_strength);
        // ...
        if (should_be_active && !animation->is_active()) {
            animation->activate();
        }
        // ...
    });
```

This means that even with an event bus, we are still checking the trigger conditions for all animations 60 times per second, which misses the key efficiency gain of a true event-driven design.

---

## Phase 3: Transition to a Full Event-Driven Model (To-Do)

The next step is to move the trigger logic from the `AnimationManager` into the individual animations themselves, allowing them to subscribe to the specific events that concern them.

### Next Steps:

#### 1. **Refactor `Animation::bind_events`**

The responsibility for subscribing to *triggering* events must move from the `AnimationManager` to the individual animation classes. The `bind_events` virtual method is the perfect place for this.

*   **Action:** Modify each animation class to override `bind_events`. Inside this method, the animation will read its own `config` and subscribe to the appropriate events.

*   **Example for a Beat-Triggered Animation (`LightningWaveAnimation`):**

    ```cpp
    // In LightningWaveAnimation.cpp
    void LightningWaveAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) override {
        // 1. Subscribe to the beat event for ACTIVATION
        if (config.trigger_beat_min > 0.0f) {
            bus.subscribe<events::BeatDetectedEvent>([this, config](const events::BeatDetectedEvent& event) {
                if (event.strength >= config.trigger_beat_min && event.strength <= config.trigger_beat_max) {
                    this->activate(); // Activate based on a specific event
                }
            });
        }

        // 2. Subscribe to the frame update for ONGOING LOGIC (if active)
        bus.subscribe<events::FrameUpdateEvent>([this](const events::FrameUpdateEvent& event) {
            if (this->is_active()) {
                this->update(event.delta_time, event.metrics, event.bands, event.beat_strength);
            }
        });
    }
    ```

#### 2. **Remove Trigger Logic from `AnimationManager`**

Once the animations handle their own trigger subscriptions, the centralized checking logic in the `AnimationManager` must be removed.

*   **Action:** Delete the `register_animation_callbacks` method from `AnimationManager.cpp`.

*   **Action:** Simplify the `load_animations` loop in `AnimationManager.cpp` to only call the animation's `bind_events` method.

    ```cpp
    // New, simpler loop in AnimationManager::load_animations
    if (new_animation) {
        new_animation->init(nc, app_config);
        // The animation is now responsible for its own event binding
        new_animation->bind_events(anim_config, event_bus_); 
        animations_.push_back({std::move(new_animation), anim_config});
    }
    ```

#### 3. **(Optional) Introduce More Granular Events**

To further improve efficiency, we can introduce more specific events so that animations don't even need to process raw data.

*   **Suggestion:** Create a `BandEnergyAboveThresholdEvent` that is only published by the `DspEngine` when a band's energy crosses a certain threshold. Animations could then subscribe to this event instead of processing the entire `bands` vector on every frame.

By completing these steps, the project will have a truly efficient, scalable, and flexible event-driven architecture.
