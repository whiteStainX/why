#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>



namespace why {

struct AudioCaptureConfig {
    bool enabled = true;
    std::uint32_t sample_rate = 48000;
    std::uint32_t channels = 2;
    std::size_t ring_frames = 8192;
    std::string device;
    float input_gain = 1.0f;
    bool system = false;
};

struct AudioFileConfig {
    bool enabled = true;
    std::string path;
    std::uint32_t channels = 1;
    float gain = 1.0f;
};

struct AudioConfig {
    AudioCaptureConfig capture;
    AudioFileConfig file;
    bool prefer_file = false;
};

struct DspConfig {
    std::size_t fft_size = 1024;
    std::size_t hop_size = 256;
    std::size_t bands = 32;
    std::string window = "hann";
    float smoothing_attack = 0.2f;
    float smoothing_release = 0.05f;
    float beat_sensitivity = 1.0f;
    bool enable_flux = true;
};

struct VisualConfig {

    double target_fps = 60.0;

};

struct RuntimeConfig {
    bool show_metrics = true;
    bool allow_resize = true;
    bool beat_flash = true;
    bool show_overlay_metrics = false; // New config option, default to false
};

struct PluginConfig {
    std::string directory = "plugins";
    std::vector<std::string> autoload;
    bool safe_mode = false;
};

struct AnimationConfig {
    std::string type;
    int z_index = 0;
    bool initially_active = true; // New: whether the animation starts active
    // Trigger conditions
    int trigger_band_index = -1; // -1 means no band-specific trigger
    float trigger_threshold = 0.0f; // Threshold for band energy or beat strength
    float trigger_beat_min = 0.0f; // Minimum beat strength to activate
    float trigger_beat_max = 1.0f; // Maximum beat strength to activate
    // Add more generic parameters as needed, e.g., std::map<std::string, std::string> params;
};

struct AppConfig {
    std::string log_level = "info";
    AudioConfig audio;
    DspConfig dsp;
    VisualConfig visual;
    RuntimeConfig runtime;
    PluginConfig plugins;
    std::vector<AnimationConfig> animations;
};

struct ConfigLoadResult {
    AppConfig config;
    std::vector<std::string> warnings;
    bool loaded_file = false;
};

ConfigLoadResult load_app_config(const std::string& path);



} // namespace why

