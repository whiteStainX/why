# Developer's Guide: Creating New Animations

Welcome, creator! This guide provides a framework for developing new animations for the `why` audio visualizer. By following these principles, you can focus on the creative aspects of your animation while the system handles the boilerplate.

Our architecture is event-driven. This means your animation subscribes to events it cares about and only runs its logic when those events occur. This is efficient and keeps your code clean and decoupled.

---

## 1. File Setup

1.  **Create Your Files:** In `src/animations/`, create your header and source files:
    *   `my_animation.h`
    *   `my_animation.cpp`

2.  **Define Your Class:** In your header, define your animation class, inheriting from `why::animations::Animation`.

    ```cpp
    // src/animations/my_animation.h
    #pragma once

    #include "animation.h"

    namespace why::animations {

    class MyAnimation : public Animation {
    public:
        // ... Constructor, Destructor ...

        // --- Core Animation Interface ---
        void init(notcurses* nc, const AppConfig& config) override;
        void bind_events(const AnimationConfig& config, events::EventBus& bus) override;
        void update(float dt, const AudioMetrics& m, const std::vector<float>& b, float beat) override;
        void render(notcurses* nc) override;
        // ... activate(), deactivate(), etc. ...
    };

    }
    ```

3.  **Register Your Animation:** Open `src/animations/animation_manager.cpp`. In `load_animations`, add your new class to the `if/else if` chain. This allows the manager to construct your animation when it sees its type name in the config.

    ```cpp
    // In AnimationManager::load_animations
    } else if (cleaned_type == "MyAnimation") { // Add this line
        new_animation = std::make_unique<MyAnimation>();
    }
    ```

---

## 2. Configuration (`why.toml`)

To make your animation customizable, add parameters to `why.toml` and the C++ config structs.

1.  **Add to `config.h`:** Open `src/config.h` and add your new parameter to the `AnimationConfig` struct.

    ```cpp
    // In struct AnimationConfig
    float my_animation_speed = 10.0f; // Add your parameter with a default value
    ```

2.  **Add to `why.toml`:** Users can now override this value in their config file.

    ```toml
    [[animations]]
    type = "MyAnimation"
    my_animation_speed = 25.0
    ```

3.  **Use in Your Code:** In your animation's `init` method, read the value from the config. You will need to loop through the animations in the `app_config` to find your own configuration block.

---

## 3. Event Handling: The Core of Your Animation

Your animation's logic is driven by events. The `bind_events` method is where you subscribe to the events you care about. The `DspEngine` publishes several useful events for you.

### Available DSP Events (from `src/events/audio_events.h`)

*   `BeatDetectedEvent`: Fires when a beat is detected. Contains `strength`.
*   `SpectralFluxEvent`: Represents the change in the spectrum from the last frame.
*   `SpectralNoveltyEvent`: A high-level event that fires when the *texture* or *timbre* of the audio changes significantly (e.g., a new instrument or vocal track appears).

### Choosing Your Triggering Strategy

#### Option 1 (Recommended): Subscribe to Specific DSP Events

This is the most efficient and robust approach. Subscribe directly to the audio phenomenon you want to react to.

**Example:** An animation that triggers on a significant change in the music's texture.

```cpp
// In MyNoveltyAnimation.cpp
void MyNoveltyAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // Subscribe ONLY to the novelty event.
    bus.subscribe<events::SpectralNoveltyEvent>([this, config](const auto& event) {
        if (event.strength > config.my_trigger_threshold) {
            this->activate(); // Activate based on a meaningful audio event
        }
    });

    // Also subscribe to FrameUpdateEvent for smooth visual updates if needed.
    bus.subscribe<events::FrameUpdateEvent>([this](const auto& event) {
        if (this->is_active()) {
            this->update(event.delta_time); // e.g., move the animation
        }
    });
}
```

#### Option 2: Use the Generic Frame Update Helper

If your animation's logic truly depends on the raw FFT `bands` data from every single frame, you can use the standard helper. This is less efficient but useful for visualizations like a bar graph.

```cpp
// In MyBarGraphAnimation.cpp
#include "animation_event_utils.h" // Include the helper

void MyBarGraphAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // This automatically handles activation based on config and calls update() on every frame.
    bind_standard_frame_updates(this, config, bus);
}
```

---

## 4. Rendering and Using Assets

*   **Rendering Plane:** In your `init` method, create an `ncplane` for your animation to draw on. Store it as a member variable (`ncplane* plane_ = nullptr;`) and destroy it in your destructor.

*   **Drawing:** In your `render` method, use your `plane_` to draw. Erase the plane at the start of the render function (`ncplane_erase(plane_)`).

*   **Assets:** To use an external file (e.g., for ASCII art), add a path parameter to `why.toml`, read it in your `init` method, and load the file using a C++ `ifstream`.

---

## 5. Golden Rules for Clean Development

*   **DO NOT** modify `AnimationManager`, `DspEngine`, or `main.cpp` for your animation's logic.
*   **DO** contain all logic within your animation's class.
*   **DO** use the `EventBus` to get data. Subscribe to the most specific event that meets your needs.
*   **DO** define all custom options in `config.h` and `why.toml`. Avoid hard-coded "magic numbers".
*   **DO** clean up resources. If you create an `ncplane`, destroy it in your destructor.