# Event-Driven Implementation Notes

## Overview
The animation pipeline now follows the event-driven architecture proposed in
`EVENT_DRIVEN_DESIGN.md`. A dedicated `EventBus` enables publishers to emit
semantic events without needing to know who consumes them, while animations
express their interests by subscribing to the events that matter to them. This
section documents the concrete implementation to help contributors extend the
system safely.

## Core Components

### `why::events::EventBus`
* Lives in `src/events/event_bus.h` and is header-only.
* Stores subscribers per event type using `std::type_index`.
* Exposes `subscribe<T>(Handler<T>)` for listeners and `publish<T>(const T&)`
  for publishers.
* Provides `reset()` so the animation manager can drop stale subscriptions when
  reloading animations.

### Event Types
* `FrameUpdateEvent` (defined in `src/events/frame_events.h`) bundles
  `delta_time`, `AudioMetrics`, FFT band data, and `beat_strength`.
* `BeatDetectedEvent` provides a lightweight way to react specifically to beat
  intensity changes. It is emitted alongside frame updates and is available for
  future extensions such as beat-synchronised animations or plugins.

## Animation Manager Integration

### Loading Animations
`AnimationManager::load_animations` now performs the following steps:
1. Clears existing animations and resets the event bus to discard previous
   subscriptions.
2. Instantiates each animation from configuration, calls `init`, and lets the
   animation bind custom listeners via the optional `bind_events` hook.
3. Wraps the animation in a `ManagedAnimation` owned by
   `std::unique_ptr` (stable address), lets it subscribe to any custom events
   with access to its `AnimationConfig`, and registers frame-update callbacks on
   the event bus.

### Event-Driven Updates
* `update_all` no longer loops through animations. Instead it publishes:
  1. `BeatDetectedEvent` (for specialised listeners).
  2. `FrameUpdateEvent`, which drives the common animation flow.
* Each managed animation subscribes to `FrameUpdateEvent` via a lambda that:
  - Evaluates the configured beat/band trigger conditions.
  - Activates or deactivates the animation when the trigger state changes.
  - Invokes `animation->update(...)` only while the animation remains active.
* Rendering remains in `render_all`, but sorting and plane management now
  operate on `std::unique_ptr<ManagedAnimation>`.

### Hooks for Animations
* `Animation` exposes a default `bind_events(const AnimationConfig&, EventBus&)`
  method. Derived animations can override it to subscribe to additional events
  using their configuration data without touching the manager.
* This keeps behaviour local to each animation and supports future
  event types (e.g., user input) without modifying central infrastructure.

## Extending the System
1. **Add a new event** by creating a POD struct (preferably in
   `src/events/frame_events.h` or a new header) and publish it from the
   relevant subsystem.
2. **Subscribe** within an animation either in `bind_events` (for
   animation-specific concerns) or by adding another manager-level registration
   if the behaviour should remain generic.
3. **Maintain decoupling** by avoiding direct calls between emitters and
   subscribers. Instead, model the interaction as an event so that unrelated
   components stay untouched.

## Validation Against the Design
* The implementation realises the event-driven proposal: emitters publish
  events (`update_all` now acts purely as an event dispatcher) and animations
  subscribe to the data streams they require.
* Trigger evaluation moved from the render loop into event callbacks, removing
  tight coupling between the loop and individual animation logic.
* The structure keeps functionality unchanged—activation conditions and update
  semantics match the previous behaviour—while improving extensibility and
  testability.


