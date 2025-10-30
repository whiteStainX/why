# why

Same same but different, no fork but copy, from myself, but why?

The app itself is getting more modern, I believe so.

## Prerequisites

Ensure the following tools are available on your system:

- A C++20-compatible compiler (e.g., `g++-10` or newer, `clang++-12` or newer)
- [CMake](https://cmake.org/) 3.16 or later
- [notcurses](https://github.com/dankamongmen/notcurses) development libraries
  - Ubuntu/Debian: `sudo apt install libnotcurses-dev`
  - macOS (Homebrew): `brew install notcurses`

The `external/` directory already includes the single-header/minimal sources for `miniaudio` and `kissfft` used in later phases.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

After a successful build, run the executable from the repository root:

```bash
./build/why [--config path/to/why.toml] [--file path/to/audio.wav] [--system] [--mic] [--device "name"]
```

Running without flags opens the real-time capture path (requires microphone permissions). Supplying `--file` (or `-f`) streams audio from disk through the same DSP chain. Supported formats depend on miniaudio's decoder (WAV/MP3/FLAC and more). The file path option downmixes to mono, resamples to 48 kHz, and feeds the visualizer at real-time speed so you can test the visualization without capture hardware. Use `--config` (or `-c`) to load an alternate TOML configuration. The new capture switches behave as follows:

- `--system`: Request loopback/system audio capture (platform specific requirements below).
- `--mic`: Force microphone capture even if the configuration enables system capture.
- `--device "name"`: Lock capture to a specific device label reported by miniaudio (case-insensitive substring match). Combine with `--system` when you want a non-default loopback/monitor source.

You can set the same preferences persistently through `[audio.capture]` in `why.toml` (`device = "..."`, `system = true`).

### System audio capture

To visualise only what the system is playing (Spotify, YouTube, games, etc.) configure per platform:

- **Windows**: `--system` uses WASAPI loopback on the default playback device—no extra drivers needed.
- **macOS**:
  1. Install the free virtual driver: `brew install --cask blackhole-2ch`.
  2. In _Audio MIDI Setup_, create a _Multi-Output Device_ that contains both **BlackHole 2ch** and your speakers.
  3. Set the Multi-Output Device as the system output, then run `./build/why --system` (miniaudio will expose “BlackHole” as an input device).
     > For advanced setup with specific terminals like `cool-retro-term`, see `MacOS.md`.
- **Linux (PulseAudio/PipeWire)**: `--system` auto-selects the default sink monitor (name ends with `.monitor`). To pick another source, list them via `pactl list sources short` and pass `--device <monitor name>`.

If the helper cannot locate the required loopback/monitor device, the program prints guidance on how to fix the environment.
