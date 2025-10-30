# Developer's Guide: Creating New Animations

Welcome, creator! This guide provides a framework for developing new animations for the `why` audio visualizer. By following these principles, you can focus on the creative aspects of your animation while the system handles the boilerplate.

Our architecture is event-driven. This means your animation doesn't need to constantly check for data; instead, it subscribes to events and reacts when they happen. This is efficient and keeps your code clean and decoupled.

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
        // Constructor, Destructor
        MyAnimation();
        ~MyAnimation() override;

        // --- Core Animation Interface ---
        void init(notcurses* nc, const AppConfig& config) override;
        void bind_events(const AnimationConfig& config, events::EventBus& bus) override;
        void update(float dt, const AudioMetrics& m, const std::vector<float>& b, float beat) override;
        void render(notcurses* nc) override;
        void activate() override;
        void deactivate() override;
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

    ```cpp
    // In MyAnimation::init(...)
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "MyAnimation") {
            this->speed_ = anim_config.my_animation_speed; // Read the value
            break;
        }
    }
    ```

---

## 3. Event Handling: The Core of Your Animation

Your animation's logic is driven by events. The `bind_events` method is where you subscribe to the events you care about.

### The Easy Way: Standard Triggers

If your animation should activate/deactivate based on the standard `trigger_beat_*` and `trigger_band_*` parameters in `why.toml`, you can use a simple helper function.

```cpp
// In MyAnimation.cpp
#include "animation_event_utils.h" // Include the helper

void MyAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // This automatically handles activation, deactivation, and calls update() for you.
    bind_standard_frame_updates(this, config, bus);
}
```

With this single line, your animation will respect the standard trigger configurations. Your main job is to implement the `update()` and `render()` methods.

### The Advanced Way: Custom Triggers

For more efficient or unique animations, you can subscribe to specific events yourself. This gives you full control.

**Example:** An animation that only creates a flash effect on a strong beat and does nothing else.

```cpp
// In MyFlashAnimation.cpp
void MyFlashAnimation::bind_events(const AnimationConfig& config, events::EventBus& bus) {
    // Subscribe ONLY to the beat event.
    bus.subscribe<events::BeatDetectedEvent>([this, config](const events::BeatDetectedEvent& event) {
        if (event.strength > config.trigger_threshold) {
            this->do_flash_effect(); // Call a custom method
        }
    });

    // Note: We DO NOT subscribe to FrameUpdateEvent, making this very efficient.
    // The update() method for this animation might even be empty.
}
```

---

## 4. Rendering and Using Assets

*   **Rendering Plane:** In your `init` method, you should create an `ncplane` for your animation to draw on. Store it as a member variable (`ncplane* plane_ = nullptr;`). Remember to call `ncplane_destroy(plane_)` in your destructor!

*   **Drawing:** In your `render` method, use your `plane_` to draw. You can use functions like `ncplane_putstr_yx`, `ncplane_set_fg_rgb8`, etc. Erase the plane at the start of the render function (`ncplane_erase(plane_)`).

*   **Assets:** To use an external file (e.g., for ASCII art):
    1.  Place the file in the `assets/` directory.
    2.  Add a configuration parameter to `why.toml` for the file path (e.g., `my_art_file = "assets/my_art.txt"`).
    3.  In your `init` method, read the path from the config and use a standard C++ `ifstream` to load the file content.

---

## 5. Golden Rules for Clean Development

To ensure the project remains stable and easy to maintain, please follow these rules:

*   **DO NOT** modify `AnimationManager`, `DspEngine`, or `main.cpp` for your animation's logic.
*   **DO** contain all logic within your animation's class.
*   **DO** use the `EventBus` to get data. Never try to get a direct pointer to another component like the `DspEngine`.
*   **DO** define all custom options in `config.h` and `why.toml`. Avoid hard-coded "magic numbers" in your animation logic.
*   **DO** clean up resources. If you create an `ncplane`, destroy it in your destructor.
