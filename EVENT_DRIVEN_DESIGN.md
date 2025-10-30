# Event-Driven Architecture

This document describes the event-driven architecture of the `why` audio visualizer's animation system. This design improves efficiency, promotes strong decoupling between components, and enhances flexibility.

---

## 1. System Overview

Instead of a centralized manager polling every animation for state changes, the system is built around an **Event Bus**. Components are decoupled into three main roles:

*   **Emitters (Publishers):** Components that generate data and publish it to the Event Bus as a named event. They have no knowledge of who, if anyone, is listening.
*   **Event Bus (Dispatcher):** A central hub that receives all published events and forwards them to any subscribers registered for that specific event type.
*   **Subscribers (Listeners):** Components (primarily animations) that subscribe to the specific events they care about. They execute their logic only when they receive a relevant event.

### Architecture Diagram

```
+-----------+      publishes       +--------------------+      notifies      +--------------------+
| DspEngine |--------------------->|                    |------------------->| Animation 1        |
| (Emitter) |   BeatDetectedEvent  |                    |                    | (Subscribes to Beat)|
+-----------+                      |                    |                    +--------------------+
                                   |     Event Bus      |
                                   |                    |                    +--------------------+
+------------------+   publishes   |                    |      notifies      | Animation 2        |
| AnimationManager |-------------->|                    |------------------->| (Subscribes to     |
| (Emitter)        | FrameUpdateEvent |                    |                    |  FrameUpdate)      |
+------------------+               +--------------------+                    +--------------------+
```

### Key Advantages

*   **Efficiency:** An animation's code is only executed when an event it has subscribed to occurs. An animation that only reacts to strong beats will consume no resources on frames where no beat is detected.
*   **Decoupling:** Emitters and subscribers are completely independent. The `DspEngine` doesn't know what animations exist, and animations don't know where the audio data comes from.
*   **Extensibility:** New animations can be added without modifying any central manager. They simply subscribe to the events they need. New event types can also be added without affecting existing components.

---

## 2. Core Components & Event Flow

### Emitters
*   **`AnimationManager` (in the main loop):** This is the primary emitter. On every frame, it publishes:
    1.  `BeatDetectedEvent`: Contains the current beat strength.
    2.  `FrameUpdateEvent`: A larger event containing `delta_time`, `AudioMetrics`, and the full vector of FFT band data.

### Event Flow Example: A Beat-Triggered Animation

1.  The `main` loop gets the latest beat strength from the `DspEngine`.
2.  It calls `animation_manager.update_all()`, passing in the data.
3.  The `AnimationManager` publishes a `BeatDetectedEvent` to the `EventBus`.
4.  The `EventBus` checks its list of subscribers for this event type.
5.  It finds that `LightningWaveAnimation` has subscribed to it.
6.  It invokes the handler function provided by `LightningWaveAnimation`, which then runs its activation logic.
7.  Other animations that did not subscribe are not affected and their code is not executed.

---

## 3. Implementation for Developers

The key to integrating a new animation into this system is the `bind_events` method, which is part of the `Animation` interface.

### `Animation::bind_events`

This virtual method is called once when your animation is loaded. Its purpose is to subscribe your animation to all the events it needs to function.

#### The Easy Way: Standard Triggers

For animations that use the common beat/band trigger conditions from `why.toml`, a helper function is provided.

```cpp
// In MyAnimation.cpp
#include "animation_event_utils.h"

void MyAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // This one line handles activation, deactivation, and calls update() for you.
    bind_standard_frame_updates(this, config, bus);
}
```

#### The Advanced Way: Custom Subscriptions

For more efficient or unique animations, you can subscribe to events directly. This gives you precise control.

```cpp
// For an animation that only flashes on a beat.
void MyFlashAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // Subscribe ONLY to the beat event.
    bus.subscribe<events::BeatDetectedEvent>([this, config](const events::BeatDetectedEvent& event) {
        if (event.strength > config.trigger_threshold) {
            this->do_flash_effect();
        }
    });
    // This animation is now highly efficient, as its code only runs when a beat occurs.
}
```