# Project Architecture: `why` Audio Visualizer

This document outlines the high-level architecture of the `why` audio visualizer, focusing on the separation of concerns between audio processing, digital signal processing (DSP), rendering, configuration, and plugins.

## Core Modules and Their Responsibilities

The application is structured into several distinct modules, each with a clear responsibility:

*   **`main.cpp`**: The entry point of the application. It handles command-line argument parsing, initializes all core modules, manages the main application loop, processes user input, and orchestrates the data flow between audio, DSP, and rendering.
*   **`AudioEngine` (audio_engine.h/cpp)**: Responsible for all audio input and management. This includes capturing audio from a microphone or system audio, or streaming from a file. It provides raw audio samples to the DSP engine.
*   **`DspEngine` (dsp.h/cpp)**: Performs digital signal processing on the raw audio samples. Its primary functions are Fast Fourier Transform (FFT) to analyze frequency bands, and beat detection. It outputs processed audio data (band energies, beat strength).
*   **`Renderer` (renderer.h/cpp)**: The `Renderer` module acts as the **orchestration layer** for all visual output. It coordinates the rendering of various animations to the terminal using the `notcurses` library. It receives processed audio data from the `DspEngine` and delegates the actual drawing to active `Animation` instances, managing their lifecycle and Z-ordering.
*   **`Animation` Interface (src/animations/animation.h)**: Defines the contract for all individual visual effects. Concrete animation classes inherit from this interface, implementing their specific rendering logic. This allows the `Renderer` to interact with diverse animations polymorphically.
*   **`Config` (config.h/cpp)**: Manages application configuration. It loads settings from a TOML-like configuration file (`why.toml`) and provides access to these settings throughout the application.
*   **`PluginManager` (plugins.h/cpp)**: Provides an extensible plugin system. It loads and manages plugins that can react to audio and DSP events, allowing for custom behaviors or debugging tools.

## Data Flow and Interactions (ASCII Diagram)

```
+-----------------+     +-----------------+     +-----------------+
|     main.cpp    |     |   AudioEngine   |     |    DspEngine    |
| (Orchestration) |     | (Audio Input)   |     | (Audio Analysis)|
+--------+--------+     +--------+--------+     +--------+--------+
         |                       ^                       ^
         |                       |                       | 
         |  1. Init/Config       |  3. Raw Audio Samples |  4. Processed Audio Data
         |                       |                       | (Band Energies, Beat Strength)
         |                       |                       | 
         v                       |                       | 
+--------+--------+     +--------+--------+     +--------+--------+
|     Config      |     |   AudioEngine   |     |    DspEngine    |
| (Load Settings) |     | (Capture/Stream)|     | (FFT, Beat Det.)|
+--------+--------+     +--------+--------+     +--------+--------+
         |                       |                       | 
         |  2. AppConfig         |                       | 
         |                       |                       | 
         v                       v                       v
+--------+--------+     +--------+--------+     +-----------------+
|  PluginManager  |     |     main.cpp    |     |     Renderer    |
| (Event Notif.)  |     | (Main Loop)     |     | (Orchestration) |
+--------+--------+     +--------+--------+     +--------+--------+
         ^                       |                       | 
         |                       |                       | 
         |  5. Events            |  6. Render Call       |  7. Animation Management
         | (AudioMetrics, Bands, | (Notcurses Context,   | (Z-order, Lifecycle)
         |  Beat Strength, Time) |  DSP Data)            |                       | 
         |                       |                       v                       | 
         +-----------------------+             +-----------------+
                                               |    Animation    |
                                               |    (Interface)  |
                                               +--------+--------+
                                                        ^
                                                        |
                                                        |
                                                        | 8. Specific Animation Logic
                                                        |
                                               +--------+--------+
                                               |  RandomTextAnim |
                                               |  (Concrete Impl)|
                                               +-----------------+
```

### Explanation of Data Flow:

1.  **Initialization and Configuration (`main.cpp` -> `Config`):**
    *   `main.cpp` starts by loading application settings from `why.toml` using the `Config` module.
    *   The `Config` module parses the file and provides an `AppConfig` object.

2.  **Audio Engine Setup (`main.cpp` -> `AudioEngine`):**
    *   `main.cpp` initializes the `AudioEngine` with parameters from `AppConfig` (sample rate, channels, device, file path).
    *   The `AudioEngine` starts capturing or streaming audio.

3.  **Audio Data to DSP (`AudioEngine` -> `DspEngine` via `main.cpp`):**
    *   In the main loop, `main.cpp` continuously reads raw audio samples from the `AudioEngine`'s ring buffer.
    *   These raw samples are then pushed to the `DspEngine`.

4.  **DSP Processing (`DspEngine`):**
    *   The `DspEngine` processes the incoming raw audio samples.
    *   It performs FFT to calculate frequency band energies and detects beats.
    *   It maintains internal state for smoothing and beat detection.

5.  **Plugin Notifications (`main.cpp` -> `PluginManager`):**
    *   After DSP processing, `main.cpp` notifies the `PluginManager` with the latest `AudioMetrics`, band energies, beat strength, and elapsed time.
    *   Active plugins (loaded via `AppConfig`) can then react to these events (e.g., `BeatFlashDebugPlugin` logs beat events).

6.  **Rendering (`main.cpp` -> `Renderer`):**
    *   `main.cpp` calls the `render_frame` function in the `Renderer` module.
    *   It passes the `notcurses` context and processed audio data.

7.  **Animation Management (`Renderer` -> `Animation`):**
    *   The `Renderer` (acting as an orchestrator) manages active `Animation` instances. It calls the `render` method on the currently active animation.
    *   It is responsible for managing the Z-order of `ncplane`s used by different animations.

8.  **Specific Animation Logic (`Animation` Implementations):**
    *   Concrete `Animation` classes (e.g., `RandomTextAnimation`) implement the actual visual effects, drawing to their assigned `ncplane`s based on the provided audio data and other parameters.

## Module APIs (High-Level)

### `AudioEngine`
*   **Constructor**: `AudioEngine(sample_rate, channels, ring_frames, file_path, device_name, system_audio)`
*   `start()`: Initializes and starts audio capture/streaming.
*   `stop()`: Stops audio capture/streaming and cleans up resources.
*   `read_samples(dest_buffer, max_samples)`: Reads processed audio samples into a buffer.
*   `dropped_samples()`: Returns the count of dropped audio samples.
*   `last_error()`: Returns the last error message.
*   `channels()`: Returns the number of audio channels.
*   `using_file_stream()`: Indicates if audio is from a file stream.

### `DspEngine`
*   **Constructor**: `DspEngine(sample_rate, channels, fft_size, hop_size, bands)`
*   `push_samples(interleaved_samples, count)`: Feeds raw audio samples for processing.
*   `band_energies()`: Returns a vector of current frequency band energies.
*   `beat_strength()`: Returns the current beat strength.

### `Renderer`
*   `render_frame(notcurses* nc, float time_s, const AudioMetrics& metrics, const std::vector<float>& bands, float beat_strength, bool file_stream, bool show_metrics, bool show_overlay_metrics)`: The main rendering orchestrator.
*   `set_active_animation(std::unique_ptr<animations::Animation> animation)`: Sets the animation to be rendered.

### `Animation` (Interface)
*   `virtual ~Animation() = default;`
*   `virtual void render(notcurses* nc, float time_s, const AudioMetrics& metrics, const std::vector<float>& bands, float beat_strength) = 0;`

### `Config`
*   `load_app_config(path)`: Loads configuration from a file, returning `AppConfig` and any warnings.

### `PluginManager`
*   `register_factory(id, factory_function)`: Registers a plugin factory.
*   `load_from_config(app_config)`: Loads plugins specified in the configuration.
*   `notify_frame(audio_metrics, bands, beat_strength, time_s)`: Notifies active plugins of new frame data.
*   `warnings()`: Returns any warnings encountered during plugin loading.

## Future Development Guidelines

*   **Strict Module Boundaries:** When adding new features or modifying existing ones, ensure that changes adhere to the defined responsibilities of each module. Avoid introducing cross-module dependencies that blur these lines.
*   **API-First Design:** Before implementing new functionality, define the public API of the module that will provide it. This promotes clear interfaces and reduces coupling.
*   **Testability:** Design modules to be easily testable in isolation. This often means minimizing global state and providing clear inputs/outputs.
*   **Extensibility:** The `PluginManager` is a good example of extensibility. Consider similar patterns for other parts of the application where custom behavior might be desired (e.g., custom DSP algorithms, new rendering backends).
*   **Animation Orchestration (DAG & Z-Ordering):** The `Renderer` module, acting as an orchestrator, will manage a Directed Acyclic Graph (DAG) of animations. This allows animations to be conditionally triggered, sequenced, and composed based on application state, audio events, or other animation states. Notcurses' `ncplane` system will be leveraged for efficient Z-ordering, allowing animations to be layered on top of each other. Each animation can potentially manage its own `ncplane` for drawing, with the orchestrator controlling the plane's position in the Z-stack.
*   **Enhanced Animation Interface:** Consider extending the `Animation` interface with methods like `init()`, `update(float delta_time)`, `is_active()`, `get_z_index()`, and `get_plane()` to provide more granular control over animation lifecycle, state, and layering.
*   **Animation Manager:** A dedicated `AnimationManager` class (potentially within the `Renderer` module or as a standalone component) should be developed to handle the collection, lifecycle, and dependency management of `Animation` instances. This manager would be responsible for evaluating the animation DAG, updating active animations, and ensuring correct Z-ordering of their respective `ncplane`s.
*   **Animation Composition and Events:** Explore mechanisms for composing simpler animations into more complex ones. Implement an event-driven system where animations can react to specific audio events (e.g., beat detection, peak levels) or internal animation events, promoting loose coupling.
*   **Configuration of Animations:** Extend the `Config` module to allow configuration of animation properties, including which animations are active, their parameters, and their initial Z-order.
*   **Performance Considerations:** For real-time audio processing and rendering, performance is critical. Profile and optimize hot paths, especially in `AudioEngine`, `DspEngine`, and `Renderer`.
*   **Error Handling:** Implement robust error handling and reporting, especially for external dependencies like audio devices and `notcurses`.
*   **Documentation:** Keep this `DESIGN.md` up-to-date with any significant architectural changes. Add comments to code where the logic is complex or non-obvious.