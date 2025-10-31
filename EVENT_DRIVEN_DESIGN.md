# Event-Driven Architecture

This document describes the event-driven architecture of the `why` audio visualizer. This design promotes strong decoupling, efficiency, and flexibility.

---

## 1. System Overview

The architecture is built around an **Event Bus** that decouples components into three roles:

*   **Emitters (Publishers):** Components that generate data and publish it as a named event. They have no knowledge of who is listening.
*   **Event Bus (Dispatcher):** A central hub that receives all events and forwards them to any subscribers for that event type.
*   **Subscribers (Listeners):** Components (primarily animations) that subscribe to the specific events they care about and execute their logic only when they receive a relevant event.

### Architecture Diagram

```
+-----------+      publishes       +--------------------+      notifies      +--------------------+
| DspEngine |--------------------->|                    |------------------->| Animation 1        |
| (Emitter) | (Beat, Flux, etc.)   |                    |                    | (Subscribes to     |
+-----------+                      |                    |                    |  specific events)  |
                                   |     Event Bus      |                    +--------------------+
                                   |                    |
+------------------+   publishes   |                    |      notifies      +--------------------+
| AnimationManager |-------------->|                    |------------------->| Animation 2        |
| (Emitter)        | FrameUpdateEvent |                    |                    | (Subscribes to     |
+------------------+               +--------------------+                    |  FrameUpdate)      |
```

### Key Advantages

*   **Efficiency:** An animation's code only runs when an event it has subscribed to occurs.
*   **Decoupling:** The `DspEngine` and `AnimationManager` are completely independent of the animations.
*   **Extensibility:** New animations can be added without modifying core systems. They simply subscribe to the events they need.

---

## 2. Core Components & Event Flow

### Emitters

*   **`DspEngine`**: The primary source of audio analysis events. After processing an audio frame, it publishes several granular events, including:
    *   `BeatDetectedEvent`
    *   `SpectralFluxEvent`
    *   `SpectralNoveltyEvent`
*   **`AnimationManager`**: Publishes the general `FrameUpdateEvent` on every frame, which provides timing and the raw FFT band data for animations that need it.

### Event Flow Example: A Novelty-Triggered Animation

1.  The `DspEngine` processes an audio frame and its novelty detection algorithm identifies a significant change in the audio's texture.
2.  It publishes a `SpectralNoveltyEvent` with a `strength` value to the `EventBus`.
3.  The `EventBus` finds that `LightningWaveAnimation` has subscribed to this event.
4.  It invokes the handler in `LightningWaveAnimation`, which then uses the `strength` to trigger its visual effect.
5.  Other animations that did not subscribe are not affected.

---

## 3. Implementation for Developers

The key to this system is the `bind_events` method in the `Animation` interface, where an animation subscribes to the events it needs.

#### Subscribing to a DSP Event

For an animation that should react to a specific audio phenomenon, subscribe directly to the event from the `DspEngine`.

```cpp
// For an animation that triggers on spectral novelty
void MyNoveltyAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    bus.subscribe<events::SpectralNoveltyEvent>([this, config](const auto& event) {
        if (event.strength > config.my_trigger_threshold) {
            this->activate();
        }
    });
}
```

#### Subscribing to Frame Updates

For animations that need to run logic on every single frame (e.g., for smooth movement), subscribe to `FrameUpdateEvent`.

```cpp
// For an animation with continuous movement
void MyMovingAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    bus.subscribe<events::FrameUpdateEvent>([this](const auto& event) {
        if (this->is_active()) {
            this->update(event.delta_time);
        }
    });
}
```
