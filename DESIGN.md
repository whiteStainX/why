# Project Architecture: `why` Audio Visualizer

This document outlines the high-level architecture of the `why` audio visualizer. The design is centered around a decoupled, event-driven model that promotes modularity and extensibility.

## Core Modules and Their Responsibilities

*   **`main.cpp`**: The application entry point. It handles command-line parsing, initializes all core modules, and runs the main application loop.
*   **`AudioEngine`**: Responsible for all audio input. It captures audio from a microphone or system, or streams from a file, providing raw audio samples.
*   **`DspEngine`**: Performs digital signal processing. Its primary functions are Fast Fourier Transform (FFT) to analyze frequency bands and beat detection.
*   **`EventBus`**: The central dispatcher for the event-driven system. It receives events from publishers and forwards them to subscribers, decoupling the components.
*   **`AnimationManager`**: Orchestrates the animation subsystem. It loads animations from the configuration, publishes audio-related events (`BeatDetectedEvent`, `FrameUpdateEvent`) to the `EventBus` on each frame, and manages the rendering of all active animations.
*   **`Animation` (Interface)**: The base class for all visual effects. Concrete animations inherit from this, implement their visual logic, and subscribe to events via the `bind_events` method.
*   **`Config`**: Manages application configuration, loading settings from `why.toml`.
*   **`PluginManager`**: Provides an extensible plugin system, allowing custom code to react to application events.

## Data Flow and Interactions (Event-Driven)

```
+-------------+   reads   +---------------+   pushes   +-----------+ 
|  main.cpp   |---------->|  AudioEngine  |----------->| DspEngine | 
| (Main Loop) |           +---------------+            +-----------+ 
+-------------+                                              |
      |                                                        |
      | 1. On each frame, calls...                             | 2. Provides
      |                                                        |    beat & band data
      v                                                        v
+------------------+  3. Publishes events   +-------------+  4. Dispatches events  +--------------------+
| AnimationManager |----------------------->|  EventBus   |--------------------->| Animation Instances|
| (Event Emitter)  | (BeatDetectedEvent,   +-------------+                      | (Subscribers)      |
+------------------+  FrameUpdateEvent)                                        +--------------------+
      |                                                                                |
      | 5. After events, calls...                                                      | 6. Update internal
      |                                                                                |    state & visuals
      v                                                                                |
+------------------+  7. Calls render() on active animations                           |
| AnimationManager |-------------------------------------------------------------------+
| (Renderer)       |
+------------------+
```

### Explanation of Data Flow:

1.  **Audio Processing:** In the main loop, `main.cpp` reads raw audio from the `AudioEngine` and pushes it to the `DspEngine` for analysis.
2.  **Data to Manager:** The `DspEngine` provides the processed data (beat strength, band energies) back to the main loop.
3.  **Event Publishing:** `main.cpp` calls `animation_manager.update_all()`, which acts as the primary event emitter. It publishes the `BeatDetectedEvent` and `FrameUpdateEvent` to the `EventBus`.
4.  **Event Dispatching:** The `EventBus` receives these events and forwards them to all `Animation` instances that have subscribed to them.
5.  **Animation State Update:** Each subscribed animation reacts to the event, updating its internal state (e.g., activating, changing position, calculating a new visual).
6.  **Rendering:** After events are published, `main.cpp` calls `animation_manager.render_all()`. The manager iterates through all active animations and calls their respective `render()` methods, which draw their current state to the screen.

## Module APIs (High-Level)

### `AnimationManager`
*   `load_animations(nc, app_config)`: Loads all animations from the config and calls their `bind_events` method.
*   `update_all(delta_time, metrics, bands, beat_strength)`: Publishes the frame's audio events to the `EventBus`.
*   `render_all(nc)`: Renders all active animations.

### `Animation` (Interface)
*   `virtual void init(nc, config)`: For one-time setup.
*   `virtual void bind_events(config, bus)`: **Crucial method** where the animation subscribes to events.
*   `virtual void update(...)`: Logic to run on a `FrameUpdateEvent` (if subscribed).
*   `virtual void render(nc)`: Draws the animation's current state.

### `EventBus`
*   `subscribe<T>(handler)`: Allows a listener to subscribe to an event of type `T`.
*   `publish<T>(event)`: Publishes an event to all registered subscribers.

### `DspEngine`
*   `push_samples(samples, count)`: Feeds raw audio for processing.
*   `band_energies()`: Returns a vector of current frequency band energies.
*   `beat_strength()`: Returns the current beat strength.

## Design Principles

*   **Decoupling:** Components should not have direct knowledge of each other. Communication should occur via the `EventBus`.
*   **Modularity:** All logic for a specific animation is contained within its own class. The `AnimationManager` does not need to know how any specific animation works.
*   **Extensibility:** New animations can be created and integrated without modifying core systems like the `DspEngine` or `AnimationManager`.
