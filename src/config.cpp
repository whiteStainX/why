#include "config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace why {
namespace {

struct RawScalar {
    std::string value;
    int line = 0;
};

struct RawArray {
    std::vector<std::string> values;
    int line = 0;
};

struct RawConfig {
    std::unordered_map<std::string, RawScalar> scalars;
    std::unordered_map<std::string, RawArray> arrays;
    std::vector<std::unordered_map<std::string, RawScalar>> animation_configs;
};

std::string ltrim(std::string_view sv) {
    std::size_t idx = 0;
    while (idx < sv.size() && std::isspace(static_cast<unsigned char>(sv[idx]))) {
        ++idx;
    }
    return std::string(sv.substr(idx));
}

std::string rtrim(std::string_view sv) {
    std::size_t idx = sv.size();
    while (idx > 0 && std::isspace(static_cast<unsigned char>(sv[idx - 1]))) {
        --idx;
    }
    return std::string(sv.substr(0, idx));
}

std::string trim(std::string_view sv) {
    return rtrim(ltrim(sv));
}

std::string strip_inline_comment(const std::string& value) {
    bool in_quotes = false;
    char quote_char = '\0';
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if ((c == '\"' || c == '\'') && (i == 0 || value[i - 1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (quote_char == c) {
                in_quotes = false;
            }
        }
        if (c == '#' && !in_quotes) {
            return trim(value.substr(0, i));
        }
    }
    return trim(value);
}

std::vector<std::string> parse_array_values(const std::string& raw, int line, std::vector<std::string>& warnings) {
    std::vector<std::string> values;
    std::string current;
    bool in_quotes = false;
    char quote_char = '\0';
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (in_quotes) {
            if (c == quote_char && (i == 0 || raw[i - 1] != '\\')) {
                in_quotes = false;
            } else {
                current.push_back(c);
            }
            continue;
        }
        if (c == '\"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
            continue;
        }
        if (c == ',') {
            std::string value = trim(current);
            if (!value.empty()) {
                values.push_back(value);
            }
            current.clear();
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            current.push_back(c);
        }
    }
    std::string value = trim(current);
    if (!value.empty()) {
        values.push_back(value);
    }
    if (in_quotes) {
        std::ostringstream oss;
        oss << "Unterminated string in array on line " << line;
        warnings.push_back(oss.str());
    }
    for (std::string& v : values) {
        if (!v.empty() && v.front() == '\"' && v.back() == '\"' && v.size() >= 2) {
            v = v.substr(1, v.size() - 2);
        } else if (!v.empty() && v.front() == '\'' && v.back() == '\'' && v.size() >= 2) {
            v = v.substr(1, v.size() - 2);
        }
    }
    return values;
}

std::string sanitize_string_value(const std::string& value) {
    std::string trimmed = trim(value);
    if (trimmed.length() >= 2) {
        const char first = trimmed.front();
        const char last = trimmed.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            trimmed = trimmed.substr(1, trimmed.length() - 2);
        }
    }
    return trimmed;
}

void parse_file(const std::string& path, RawConfig& out, std::vector<std::string>& warnings, bool& loaded_file) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }
    loaded_file = true;

    std::string line;
    std::string current_section;
    std::unordered_map<std::string, RawScalar>* current_animation_map = nullptr;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            if (trimmed.front() == '[' && trimmed.at(1) == '[' && trimmed.back() == ']' && trimmed.at(trimmed.size() - 2) == ']') {
                // This is an array of tables, e.g., [[animations]]
                std::string array_name = trim(trimmed.substr(2, trimmed.size() - 4));
                if (array_name == "animations") {
                    out.animation_configs.emplace_back();
                    current_animation_map = &out.animation_configs.back();
                } else {
                    current_animation_map = nullptr; // Stop collecting for animations
                }
                current_section.clear(); // Clear single-bracket section
            } else {
                // This is a single-bracket section, e.g., [audio]
                current_section = trim(trimmed.substr(1, trimmed.size() - 2));
                current_animation_map = nullptr; // Stop collecting for animations
            }
            continue;
        }

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            std::ostringstream oss;
            oss << "Ignoring line " << line_number << ": missing '='";
            warnings.push_back(oss.str());
            continue;
        }
        std::string key = trim(trimmed.substr(0, eq));
        std::string value = strip_inline_comment(trimmed.substr(eq + 1));

        if (current_animation_map) {
            RawScalar scalar;
            scalar.value = value;
            scalar.line = line_number;
            (*current_animation_map)[key] = scalar;
        } else {
            std::string full_key = key;
            if (!current_section.empty()) {
                full_key = current_section + "." + key;
            }
            if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
                std::string inner = trim(value.substr(1, value.size() - 2));
                RawArray array;
                array.values = parse_array_values(inner, line_number, warnings);
                array.line = line_number;
                out.arrays[full_key] = array;
            } else {
                RawScalar scalar;
                scalar.value = value;
                scalar.line = line_number;
                out.scalars[full_key] = scalar;
            }
        }
    }
}

bool parse_bool(const std::string& value, bool& out) {
    const std::string lower = [&]() {
        std::string temp(value);
        std::transform(temp.begin(), temp.end(), temp.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return trim(temp);
    }();
    if (lower == "true" || lower == "1" || lower == "yes") {
        out = true;
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no") {
        out = false;
        return true;
    }
    return false;
}

bool parse_int64(const std::string& value, std::int64_t& out) {
    try {
        size_t idx = 0;
        const long long parsed = std::stoll(value, &idx, 0);
        if (idx != value.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_uint64(const std::string& value, std::uint64_t& out) {
    try {
        size_t idx = 0;
        const unsigned long long parsed = std::stoull(value, &idx, 0);
        if (idx != value.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_double(const std::string& value, double& out) {
    try {
        size_t idx = 0;
        const double parsed = std::stod(value, &idx);
        if (idx != value.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_uint32(const std::string& value, std::uint32_t& out) {
    std::uint64_t parsed = 0;
    if (!parse_uint64(value, parsed)) {
        return false;
    }
    out = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_size(const std::string& value, std::size_t& out) {
    std::uint64_t parsed = 0;
    if (!parse_uint64(value, parsed)) {
        return false;
    }
    out = static_cast<std::size_t>(parsed);
    return true;
}

bool parse_float32(const std::string& value, float& out) {
    double parsed = 0.0;
    if (!parse_double(value, parsed)) {
        return false;
    }
    out = static_cast<float>(parsed);
    return true;
}

bool parse_int32(const std::string& value, int& out) {
    std::int64_t parsed = 0;
    if (!parse_int64(value, parsed)) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

template <typename T, typename Parser>
void assign_scalar(const RawConfig& raw,
                   const std::string& key,
                   T& target,
                   Parser parser,
                   std::vector<std::string>& warnings) {
    const auto it = raw.scalars.find(key);
    if (it == raw.scalars.end()) {
        return;
    }
    T parsed_value{};
    if (parser(it->second.value, parsed_value)) {
        target = parsed_value;
    } else {
        std::ostringstream oss;
        oss << "Invalid value for '" << key << "' on line " << it->second.line;
        warnings.push_back(oss.str());
    }
}

void assign_string(const RawConfig& raw, const std::string& key, std::string& target) {
    const auto it = raw.scalars.find(key);
    if (it == raw.scalars.end()) {
        return;
    }
    std::string value = it->second.value;
    if (!value.empty() && value.front() == '\"' && value.back() == '\"' && value.size() >= 2) {
        value = value.substr(1, value.size() - 2);
    }
    if (!value.empty() && value.front() == '\'' && value.back() == '\'' && value.size() >= 2) {
        value = value.substr(1, value.size() - 2);
    }
    target = value;
}

} // namespace

ConfigLoadResult load_app_config(const std::string& path) {
    ConfigLoadResult result;
    RawConfig raw;
    parse_file(path, raw, result.warnings, result.loaded_file);

    assign_string(raw, "log.level", result.config.log_level);

    assign_scalar(raw,
                  "audio.capture.enabled",
                  result.config.audio.capture.enabled,
                  parse_bool,
                  result.warnings);
    assign_scalar(raw,
                  "audio.capture.sample_rate",
                  result.config.audio.capture.sample_rate,
                  parse_uint32,
                  result.warnings);
    assign_scalar(raw,
                  "audio.capture.channels",
                  result.config.audio.capture.channels,
                  parse_uint32,
                  result.warnings);
    assign_scalar(raw,
                  "audio.capture.ring_frames",
                  result.config.audio.capture.ring_frames,
                  parse_size,
                  result.warnings);
    assign_string(raw, "audio.capture.device", result.config.audio.capture.device);
    assign_scalar(raw,
                  "audio.capture.input_gain",
                  result.config.audio.capture.input_gain,
                  parse_float32,
                  result.warnings);
    assign_scalar(raw,
                  "audio.capture.system",
                  result.config.audio.capture.system,
                  parse_bool,
                  result.warnings);

    assign_scalar(raw,
                  "audio.file.enabled",
                  result.config.audio.file.enabled,
                  parse_bool,
                  result.warnings);
    assign_string(raw, "audio.file.path", result.config.audio.file.path);
    assign_scalar(raw,
                  "audio.file.channels",
                  result.config.audio.file.channels,
                  parse_uint32,
                  result.warnings);
    assign_scalar(raw,
                  "audio.file.gain",
                  result.config.audio.file.gain,
                  parse_float32,
                  result.warnings);
    assign_scalar(raw,
                  "audio.prefer_file",
                  result.config.audio.prefer_file,
                  parse_bool,
                  result.warnings);

    assign_scalar(raw, "dsp.fft_size", result.config.dsp.fft_size, parse_size, result.warnings);
    assign_scalar(raw, "dsp.hop_size", result.config.dsp.hop_size, parse_size, result.warnings);
    assign_scalar(raw, "dsp.bands", result.config.dsp.bands, parse_size, result.warnings);
    assign_string(raw, "dsp.window", result.config.dsp.window);
    assign_scalar(raw,
                  "dsp.smoothing_attack",
                  result.config.dsp.smoothing_attack,
                  parse_float32,
                  result.warnings);
    assign_scalar(raw,
                  "dsp.smoothing_release",
                  result.config.dsp.smoothing_release,
                  parse_float32,
                  result.warnings);
    assign_scalar(raw,
                  "dsp.beat_sensitivity",
                  result.config.dsp.beat_sensitivity,
                  parse_float32,
                  result.warnings);
    assign_scalar(raw, "dsp.enable_flux", result.config.dsp.enable_flux, parse_bool, result.warnings);







    assign_scalar(raw, "visual.target_fps", result.config.visual.target_fps, parse_double, result.warnings);

    assign_scalar(raw, "runtime.show_metrics", result.config.runtime.show_metrics, parse_bool, result.warnings);
    assign_scalar(raw, "runtime.allow_resize", result.config.runtime.allow_resize, parse_bool, result.warnings);
    assign_scalar(raw, "runtime.beat_flash", result.config.runtime.beat_flash, parse_bool, result.warnings);
    assign_scalar(raw, "runtime.show_overlay_metrics", result.config.runtime.show_overlay_metrics, parse_bool, result.warnings);

    assign_string(raw, "plugins.directory", result.config.plugins.directory);
    const auto array_it = raw.arrays.find("plugins.autoload");
    if (array_it != raw.arrays.end()) {
        result.config.plugins.autoload = array_it->second.values;
    }
    assign_scalar(raw, "plugins.safe_mode", result.config.plugins.safe_mode, parse_bool, result.warnings);

    // Parse animation configurations
    for (const auto& raw_anim_config : raw.animation_configs) {
        AnimationConfig anim_config;
        const auto type_it = raw_anim_config.find("type");
        if (type_it != raw_anim_config.end()) {
            anim_config.type = sanitize_string_value(type_it->second.value);
        } else {
            std::ostringstream oss;
            oss << "Animation configuration missing 'type' for an entry.";
            result.warnings.push_back(oss.str());
            continue;
        }

        const auto z_index_it = raw_anim_config.find("z_index");
        if (z_index_it != raw_anim_config.end()) {
            parse_int32(z_index_it->second.value, anim_config.z_index);
        }

        const auto initially_active_it = raw_anim_config.find("initially_active");
        if (initially_active_it != raw_anim_config.end()) {
            parse_bool(initially_active_it->second.value, anim_config.initially_active);
        }

        const auto trigger_band_index_it = raw_anim_config.find("trigger_band_index");
        if (trigger_band_index_it != raw_anim_config.end()) {
            parse_int32(trigger_band_index_it->second.value, anim_config.trigger_band_index);
        }

        const auto trigger_threshold_it = raw_anim_config.find("trigger_threshold");
        if (trigger_threshold_it != raw_anim_config.end()) {
            parse_float32(trigger_threshold_it->second.value, anim_config.trigger_threshold);
        }

        const auto trigger_beat_min_it = raw_anim_config.find("trigger_beat_min");
        if (trigger_beat_min_it != raw_anim_config.end()) {
            parse_float32(trigger_beat_min_it->second.value, anim_config.trigger_beat_min);
        }

        const auto trigger_beat_max_it = raw_anim_config.find("trigger_beat_max");
        if (trigger_beat_max_it != raw_anim_config.end()) {
            parse_float32(trigger_beat_max_it->second.value, anim_config.trigger_beat_max);
        }

        const auto text_file_path_it = raw_anim_config.find("text_file_path");
        if (text_file_path_it != raw_anim_config.end()) {
            anim_config.text_file_path = sanitize_string_value(text_file_path_it->second.value);
        }

        const auto type_speed_it = raw_anim_config.find("type_speed_words_per_s");
        if (type_speed_it != raw_anim_config.end()) {
            parse_float32(type_speed_it->second.value, anim_config.type_speed_words_per_s);
        }

        const auto display_duration_it = raw_anim_config.find("display_duration_s");
        if (display_duration_it != raw_anim_config.end()) {
            parse_float32(display_duration_it->second.value, anim_config.display_duration_s);
        }

        const auto fade_duration_it = raw_anim_config.find("fade_duration_s");
        if (fade_duration_it != raw_anim_config.end()) {
            parse_float32(fade_duration_it->second.value, anim_config.fade_duration_s);
        }

        const auto trigger_cooldown_it = raw_anim_config.find("trigger_cooldown_s");
        if (trigger_cooldown_it != raw_anim_config.end()) {
            parse_float32(trigger_cooldown_it->second.value, anim_config.trigger_cooldown_s);
        }

        const auto max_lines_it = raw_anim_config.find("max_active_lines");
        if (max_lines_it != raw_anim_config.end()) {
            parse_int32(max_lines_it->second.value, anim_config.max_active_lines);
        }

        const auto min_y_ratio_it = raw_anim_config.find("random_text_min_y_ratio");
        if (min_y_ratio_it != raw_anim_config.end()) {
            parse_float32(min_y_ratio_it->second.value, anim_config.random_text_min_y_ratio);
        }

        const auto max_y_ratio_it = raw_anim_config.find("random_text_max_y_ratio");
        if (max_y_ratio_it != raw_anim_config.end()) {
            parse_float32(max_y_ratio_it->second.value, anim_config.random_text_max_y_ratio);
        }

        const auto log_interval_it = raw_anim_config.find("log_line_interval_s");
        if (log_interval_it != raw_anim_config.end()) {
            parse_float32(log_interval_it->second.value, anim_config.log_line_interval_s);
        }

        const auto log_loop_it = raw_anim_config.find("log_loop_messages");
        if (log_loop_it != raw_anim_config.end()) {
            parse_bool(log_loop_it->second.value, anim_config.log_loop_messages);
        }

        const auto log_border_it = raw_anim_config.find("log_show_border");
        if (log_border_it != raw_anim_config.end()) {
            parse_bool(log_border_it->second.value, anim_config.log_show_border);
        }

        const auto log_padding_y_it = raw_anim_config.find("log_padding_y");
        if (log_padding_y_it != raw_anim_config.end()) {
            parse_int32(log_padding_y_it->second.value, anim_config.log_padding_y);
        }

        const auto log_padding_x_it = raw_anim_config.find("log_padding_x");
        if (log_padding_x_it != raw_anim_config.end()) {
            parse_int32(log_padding_x_it->second.value, anim_config.log_padding_x);
        }

        const auto log_title_it = raw_anim_config.find("log_title");
        if (log_title_it != raw_anim_config.end()) {
            anim_config.log_title = sanitize_string_value(log_title_it->second.value);
        }

        const auto plane_y_it = raw_anim_config.find("plane_y");
        if (plane_y_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(plane_y_it->second.value, value)) {
                anim_config.plane_y = value;
            }
        }

        const auto plane_x_it = raw_anim_config.find("plane_x");
        if (plane_x_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(plane_x_it->second.value, value)) {
                anim_config.plane_x = value;
            }
        }

        const auto plane_rows_it = raw_anim_config.find("plane_rows");
        if (plane_rows_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(plane_rows_it->second.value, value)) {
                anim_config.plane_rows = value;
            }
        }

        const auto plane_cols_it = raw_anim_config.find("plane_cols");
        if (plane_cols_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(plane_cols_it->second.value, value)) {
                anim_config.plane_cols = value;
            }
        }

        const auto matrix_rows_it = raw_anim_config.find("matrix_rows");
        if (matrix_rows_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(matrix_rows_it->second.value, value)) {
                anim_config.matrix_rows = value;
            }
        }

        const auto matrix_cols_it = raw_anim_config.find("matrix_cols");
        if (matrix_cols_it != raw_anim_config.end()) {
            int value = 0;
            if (parse_int32(matrix_cols_it->second.value, value)) {
                anim_config.matrix_cols = value;
            }
        }

        const auto matrix_border_it = raw_anim_config.find("matrix_show_border");
        if (matrix_border_it != raw_anim_config.end()) {
            parse_bool(matrix_border_it->second.value, anim_config.matrix_show_border);
        }

        const auto glyphs_file_it = raw_anim_config.find("glyphs_file_path");
        if (glyphs_file_it != raw_anim_config.end()) {
            anim_config.glyphs_file_path = sanitize_string_value(glyphs_file_it->second.value);
        }

        const auto beat_boost_it = raw_anim_config.find("matrix_beat_boost");
        if (beat_boost_it != raw_anim_config.end()) {
            parse_float32(beat_boost_it->second.value, anim_config.matrix_beat_boost);
        }

        const auto beat_threshold_it = raw_anim_config.find("matrix_beat_threshold");
        if (beat_threshold_it != raw_anim_config.end()) {
            parse_float32(beat_threshold_it->second.value, anim_config.matrix_beat_threshold);
        }

        const auto rain_angle_it = raw_anim_config.find("rain_angle_degrees");
        if (rain_angle_it != raw_anim_config.end()) {
            parse_float32(rain_angle_it->second.value, anim_config.rain_angle_degrees);
        }

        const auto wave_speed_it = raw_anim_config.find("wave_speed_cols_per_s");
        if (wave_speed_it != raw_anim_config.end()) {
            parse_float32(wave_speed_it->second.value, anim_config.wave_speed_cols_per_s);
        }

        const auto wave_front_it = raw_anim_config.find("wave_front_width_cols");
        if (wave_front_it != raw_anim_config.end()) {
            parse_int32(wave_front_it->second.value, anim_config.wave_front_width_cols);
        }

        const auto wave_tail_it = raw_anim_config.find("wave_tail_length_cols");
        if (wave_tail_it != raw_anim_config.end()) {
            parse_int32(wave_tail_it->second.value, anim_config.wave_tail_length_cols);
        }

        const auto wave_alternate_it = raw_anim_config.find("wave_alternate_direction");
        if (wave_alternate_it != raw_anim_config.end()) {
            parse_bool(wave_alternate_it->second.value, anim_config.wave_alternate_direction);
        }

        const auto wave_direction_it = raw_anim_config.find("wave_direction_right");
        if (wave_direction_it != raw_anim_config.end()) {
            parse_bool(wave_direction_it->second.value, anim_config.wave_direction_right);
        }

        const auto breathe_points_it = raw_anim_config.find("breathe_points");
        if (breathe_points_it != raw_anim_config.end()) {
            parse_int32(breathe_points_it->second.value, anim_config.breathe_points);
        }

        const auto breathe_min_radius_it = raw_anim_config.find("breathe_min_radius");
        if (breathe_min_radius_it != raw_anim_config.end()) {
            parse_float32(breathe_min_radius_it->second.value, anim_config.breathe_min_radius);
        }

        const auto breathe_max_radius_it = raw_anim_config.find("breathe_max_radius");
        if (breathe_max_radius_it != raw_anim_config.end()) {
            parse_float32(breathe_max_radius_it->second.value, anim_config.breathe_max_radius);
        }

        const auto breathe_radius_influence_it = raw_anim_config.find("breathe_audio_radius_influence");
        if (breathe_radius_influence_it != raw_anim_config.end()) {
            parse_float32(breathe_radius_influence_it->second.value, anim_config.breathe_audio_radius_influence);
        }

        const auto breathe_smoothing_it = raw_anim_config.find("breathe_smoothing_s");
        if (breathe_smoothing_it != raw_anim_config.end()) {
            parse_float32(breathe_smoothing_it->second.value, anim_config.breathe_smoothing_s);
        }

        const auto breathe_noise_it = raw_anim_config.find("breathe_noise_amount");
        if (breathe_noise_it != raw_anim_config.end()) {
            parse_float32(breathe_noise_it->second.value, anim_config.breathe_noise_amount);
        }

        const auto breathe_rotation_it = raw_anim_config.find("breathe_rotation_speed");
        if (breathe_rotation_it != raw_anim_config.end()) {
            parse_float32(breathe_rotation_it->second.value, anim_config.breathe_rotation_speed);
        }

        const auto breathe_vertical_it = raw_anim_config.find("breathe_vertical_scale");
        if (breathe_vertical_it != raw_anim_config.end()) {
            parse_float32(breathe_vertical_it->second.value, anim_config.breathe_vertical_scale);
        }

        const auto breathe_base_pulse_it = raw_anim_config.find("breathe_base_pulse_hz");
        if (breathe_base_pulse_it != raw_anim_config.end()) {
            parse_float32(breathe_base_pulse_it->second.value, anim_config.breathe_base_pulse_hz);
        }

        const auto breathe_pulse_weight_it = raw_anim_config.find("breathe_audio_pulse_weight");
        if (breathe_pulse_weight_it != raw_anim_config.end()) {
            parse_float32(breathe_pulse_weight_it->second.value, anim_config.breathe_audio_pulse_weight);
        }

        const auto breathe_band_index_it = raw_anim_config.find("breathe_band_index");
        if (breathe_band_index_it != raw_anim_config.end()) {
            parse_int32(breathe_band_index_it->second.value, anim_config.breathe_band_index);
        }

        const auto breathe_rms_weight_it = raw_anim_config.find("breathe_rms_weight");
        if (breathe_rms_weight_it != raw_anim_config.end()) {
            parse_float32(breathe_rms_weight_it->second.value, anim_config.breathe_rms_weight);
        }

        const auto breathe_beat_weight_it = raw_anim_config.find("breathe_beat_weight");
        if (breathe_beat_weight_it != raw_anim_config.end()) {
            parse_float32(breathe_beat_weight_it->second.value, anim_config.breathe_beat_weight);
        }

        const auto breathe_band_weight_it = raw_anim_config.find("breathe_band_weight");
        if (breathe_band_weight_it != raw_anim_config.end()) {
            parse_float32(breathe_band_weight_it->second.value, anim_config.breathe_band_weight);
        }

        // Future: Parse generic parameters here

        result.config.animations.push_back(anim_config);
    }

    // Sanity checks
    if (result.config.audio.capture.sample_rate == 0) {
        result.config.audio.capture.sample_rate = 48000;
    }
    if (result.config.audio.capture.channels == 0) {
        result.config.audio.capture.channels = 2;
    }
    if (result.config.audio.capture.ring_frames == 0) {
        result.config.audio.capture.ring_frames = 8192;
    }
    if (result.config.audio.file.channels == 0) {
        result.config.audio.file.channels = 1;
    }
    if (result.config.audio.file.gain <= 0.0f) {
        result.config.audio.file.gain = 1.0f;
    }
    if (result.config.dsp.hop_size == 0) {
        result.config.dsp.hop_size = std::max<std::size_t>(1, result.config.dsp.fft_size / 4);
    }


    if (result.config.visual.target_fps <= 0.0) {
        result.config.visual.target_fps = 60.0;
    }
    if (result.config.plugins.autoload.empty()) {
        result.config.plugins.autoload.push_back("beat-flash-debug");
    }

    return result;
}





} // namespace why

