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

## Phase 2: Analysis of Current State (Completed)

The hybrid polling layer inside `AnimationManager` has been removed. Trigger evaluation now lives beside the animations that depend on it, eliminating the redundant manager-level check that previously ran for every animation on every frame.

### What Changed:
*   **Per-Animation Subscriptions:** Each animation now owns its subscriptions through its `bind_events` override, so only the animations that care about a trigger evaluate it.
*   **Shared Trigger Utilities:** Common trigger helpers were extracted to `src/animations/animation_event_utils.h`, ensuring consistent behaviour without forcing a single polling loop.

---

## Phase 3: Transition to a Full Event-Driven Model (Completed)

The migration to a fully event-driven animation layer is complete.

### Highlights:
*   **`bind_events` Overrides Everywhere:** `AsciiMatrix`, `BarVisual`, `Breathe`, `CyberRain`, `LightningWave`, `Logging`, and `RandomText` all override `bind_events` and subscribe to `FrameUpdateEvent` (and future events) directly.
*   **Manager Simplification:** `AnimationManager` now simply instantiates animations, lets them bind events, and publishes `BeatDetectedEvent` and `FrameUpdateEvent`. The old `register_animation_callbacks` helper has been deleted.
*   **Reusable Trigger Evaluation:** `animation_event_utils.h` captures the trigger evaluation logic once, so animations activate/deactivate identically to the previous behaviour while staying decoupled from the manager.

---

## Future Opportunities

The architecture now supports granular events, so specialised signals (for example, a `BandEnergyAboveThresholdEvent`) can be added as needed to further cut per-frame data processing. This remains an optional enhancement rather than a migration requirement.
