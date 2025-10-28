# Project Architecture: `why` Audio Visualizer

This document outlines the high-level architecture of the `why` audio visualizer, focusing on the separation of concerns between audio processing, digital signal processing (DSP), rendering, configuration, and plugins.

## Core Modules and Their Responsibilities

The application is structured into several distinct modules, each with a clear responsibility:

*   **`main.cpp`**: The entry point of the application. It handles command-line argument parsing, initializes all core modules, manages the main application loop, processes user input, and orchestrates the data flow between audio, DSP, and rendering.
*   **`AudioEngine` (audio_engine.h/cpp)**: Responsible for all audio input and management. This includes capturing audio from a microphone or system audio, or streaming from a file. It provides raw audio samples to the DSP engine.
*   **`DspEngine` (dsp.h/cpp)**: Performs digital signal processing on the raw audio samples. Its primary functions are Fast Fourier Transform (FFT) to analyze frequency bands, and beat detection. It outputs processed audio data (band energies, beat strength).
*   **`Renderer` (renderer.h/cpp)**: Handles all visual output. It takes processed audio data from the `DspEngine` and renders it to the terminal using the `notcurses` library. It supports various visualization modes and color palettes.
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
+--------+--------+     +--------+--------+     +--------+--------+
|  PluginManager  |     |     main.cpp    |     |     Renderer    |
| (Event Notif.)  |     | (Main Loop)     |     | (Visual Output) |
+--------+--------+     +--------+--------+     +--------+--------+
         ^                       |                       ^
         |                       |                       |
         |  5. Events            |  6. Render Call       |
         | (AudioMetrics, Bands, | (Notcurses Context,   |
         |  Beat Strength, Time) |  DSP Data, Config)    |
         |                       |                       |
         +-----------------------+-----------------------+
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
    *   `main.cpp` calls the `draw_grid` function in the `Renderer` module.
    *   It passes the `notcurses` context, grid dimensions, time, visualization mode, color palette, sensitivity, `AudioMetrics`, band energies, and beat strength.
    *   The `Renderer` uses this information to draw the visualization on the terminal.

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
*   `draw_grid(notcurses_context, grid_rows, grid_cols, time_s, mode, palette, sensitivity, audio_metrics, bands, beat_strength, file_stream, show_metrics, show_overlay_metrics)`: Renders the visualization.
*   `mode_name(mode)`: Returns string name for visualization mode.
*   `palette_name(palette)`: Returns string name for color palette.

### `Config`
*   `load_app_config(path)`: Loads configuration from a file, returning `AppConfig` and any warnings.
*   `visualization_mode_from_string(value, fallback)`: Converts string to `VisualizationMode` enum.
*   `color_palette_from_string(value, fallback)`: Converts string to `ColorPalette` enum.

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
*   **Configuration-Driven:** Leverage the `Config` module for all configurable parameters. Avoid hardcoding values that might need to change.
*   **Performance Considerations:** For real-time audio processing and rendering, performance is critical. Profile and optimize hot paths, especially in `AudioEngine`, `DspEngine`, and `Renderer`.
*   **Error Handling:** Implement robust error handling and reporting, especially for external dependencies like audio devices and `notcurses`.
*   **Documentation:** Keep this `DESIGN.md` up-to-date with any significant architectural changes. Add comments to code where the logic is complex or non-obvious.
