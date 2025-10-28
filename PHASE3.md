# Animation System Refactoring - Phase 3: Advanced Features

This document outlines the detailed steps for implementing advanced features for the animation system, focusing on DAG orchestration, configuration, and new animation types. Each step includes success criteria to ensure steady progress and early error detection.

---

## Phase 3: Advanced Features

### Step 1: Implement Animation Configuration via `why.toml` (Completed)

*   **Description**: Extend the `Config` module to allow defining animations and their properties (e.g., type, Z-index, initial state) in `why.toml`. This will enable dynamic loading and management of animations.
*   **Files Changed**: `src/config.h`, `src/config.cpp`, `src/main.cpp`, `src/animations/animation_manager.h`, `src/animations/animation_manager.cpp`, `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `src/renderer.h`, `src/renderer.cpp`.
*   **Success Criteria**:
    *   `AppConfig` includes a structure to hold animation configurations (`std::vector<AnimationConfig>`).
    *   `config.cpp` parses animation-related sections from `why.toml` into `AppConfig`.
    *   `AnimationManager` has a method `load_animations(notcurses* nc, const AppConfig& config)` that creates and initializes animations based on `AppConfig`.
    *   `main.cpp` calls `why::load_animations_from_config(nc, config)` after loading the main `AppConfig`.
    *   Project builds successfully.
    *   Adding a new animation entry to `why.toml` (e.g., `[[animations]] type = "RandomText" z_index = 0`) results in `RandomTextAnimation` being loaded and rendered.

### Step 2: Add a New Animation Type (e.g., `BarVisualAnimation`) (Completed)

*   **Description**: Create a new concrete animation class that visualizes audio bands, demonstrating the extensibility of the new system. This animation will draw a simple bar graph based on `band_energies`.
*   **Files Changed**: New files `src/animations/bar_visual_animation.h`, `src/animations/bar_visual_animation.cpp`, `src/animations/animation_manager.cpp` (to instantiate the new animation), `CMakeLists.txt` (explicitly added `bar_visual_animation.cpp`).
*   **Success Criteria**:
    *   `BarVisualAnimation` class exists, inherits from `Animation`, and implements all its virtual methods.
    *   `BarVisualAnimation::init` creates its own `ncplane`.
    *   `BarVisualAnimation::update` calculates bar heights based on `band_energies` (including normalization).
    *   `BarVisualAnimation::render` draws the bar graph to its `ncplane` using ASCII characters.
    *   `AnimationManager` can instantiate `BarVisualAnimation` based on configuration.
    *   Project builds successfully.
    *   Adding `[[animations]] type = "BarVisual" z_index = 1` to `why.toml` results in the bar visual being rendered alongside `RandomTextAnimation`. 

### Step 3: Implement Basic Z-Ordering Control (Completed)

*   **Description**: Ensure animations are rendered according to their `z_index` property, allowing control over which animation appears on top. This also includes ensuring `why.toml` changes trigger a rebuild.
*   **Files Changed**: `src/animations/animation_manager.cpp` (sorting logic and `ncplane_move_bottom`), `CMakeLists.txt` (to track `why.toml` changes).
*   **Success Criteria**:
    *   `AnimationManager::render_all` correctly sorts animations by `get_z_index()` before calling their `render()` methods.
    *   By changing `z_index` in `why.toml` for `RandomTextAnimation` and `BarVisualAnimation`, their layering order changes visually.
    *   Changes to `why.toml` now correctly trigger a rebuild of the executable.
    *   Project builds successfully.

### Step 4: Implement Reactive Audio-Triggered Animations

*   **Description**: Implement a system where animations can be activated/deactivated based on real-time audio conditions (e.g., a specific frequency band exceeding a threshold, or beat strength). This removes direct animation dependencies and focuses on audio-to-visual mapping.
*   **Files Changed**: `src/config.h`, `src/config.cpp`, `src/animations/animation.h`, `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `src/animations/bar_visual_animation.h`, `src/animations/bar_visual_animation.cpp`, `src/animations/animation_manager.h`, `src/animations/animation_manager.cpp`.
*   **Success Criteria**:
    *   `AppConfig` can store trigger conditions for animations (e.g., `trigger_band_index`, `trigger_threshold`, `trigger_beat_min`).
    *   `config.cpp` successfully parses these conditions from `why.toml`.
    *   `Animation` interface includes `activate()` and `deactivate()` methods, and `is_active()` reflects the current state.
    *   `RandomTextAnimation` and `BarVisualAnimation` correctly implement `activate()` and `deactivate()`.
    *   `AnimationManager` dynamically activates/deactivates animations based on audio input by evaluating their configured trigger conditions.
    *   Project builds successfully.
    *   When running the application, animations appear/disappear or change behavior based on the audio input (e.g., a bar visual only appears when its corresponding frequency band is active).

### Step 5: Implement Animation Composition (Optional/Future)

*   **Description**: Explore ways to combine multiple animations into a single, more complex animation. This could involve a "CompositeAnimation" that manages child animations.
*   **Files Changed**: New files `src/animations/composite_animation.h`, `src/animations/composite_animation.cpp`, `src/animations/animation_manager.cpp`.
*   **Success Criteria**:
    *   A `CompositeAnimation` class exists, inheriting from `Animation`.
    *   It can contain and manage other `Animation` instances.
    *   Project builds successfully.
    *   A composite animation can be configured and rendered.

### Step 6: Event-Driven Animation Triggers (Optional/Future)

*   **Description**: Implement a simple event system where animations can subscribe to and publish events (e.g., `BeatEvent`, `PeakEvent`).
*   **Files Changed**: New files `src/events/event_bus.h`, `src/events/event_bus.cpp`, modifications to `Animation` interface and concrete animations.
*   **Success Criteria**:
    *   An `EventBus` class exists.
    *   Animations can register as listeners for specific event types.
    *   `DspEngine` or `main.cpp` can publish events.
    *   Animations react to published events.
    *   Project builds successfully.

---
