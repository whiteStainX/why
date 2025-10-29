#include <notcurses/notcurses.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "random_text_animation.h"

namespace why {
namespace animations {

RandomTextAnimation::RandomTextAnimation()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      quote_dist_(0, 0) {}

RandomTextAnimation::~RandomTextAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
    }
}

void RandomTextAnimation::init(notcurses* nc, const AppConfig& config) {
    ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(stdplane, &plane_rows, &plane_cols);

    ncplane_options p_opts{};
    p_opts.rows = plane_rows;
    p_opts.cols = plane_cols;
    p_opts.y = 0;
    p_opts.x = 0;
    plane_ = ncplane_create(stdplane, &p_opts);
    if (!plane_) {
        // std::cerr << "[RandomTextAnimation::init] Failed to create ncplane!" << std::endl;
    } else {
        // std::clog << "[RandomTextAnimation::init] ncplane created successfully." << std::endl;
    }

    // Set configuration from config file
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "RandomText") {
            z_index_ = anim_config.z_index;
            is_active_ = anim_config.initially_active;
            trigger_band_index_ = anim_config.trigger_band_index;
            trigger_threshold_ = anim_config.trigger_threshold;
            trigger_beat_min_ = anim_config.trigger_beat_min;
            trigger_beat_max_ = anim_config.trigger_beat_max;
            if (!anim_config.text_file_path.empty()) {
                text_file_path_ = anim_config.text_file_path;
            }
            if (anim_config.type_speed_words_per_s > 0.0f) {
                type_speed_words_per_s_ = anim_config.type_speed_words_per_s;
            }
            if (anim_config.display_duration_s > 0.0f) {
                display_duration_s_ = anim_config.display_duration_s;
            }
            if (anim_config.fade_duration_s > 0.0f) {
                fade_duration_s_ = anim_config.fade_duration_s;
            }
            if (anim_config.trigger_cooldown_s >= 0.0f) {
                trigger_cooldown_s_ = anim_config.trigger_cooldown_s;
            }
            if (anim_config.max_active_lines > 0) {
                max_active_lines_ = anim_config.max_active_lines;
            }
            break;
        }
    }

    load_quotes();
}

void RandomTextAnimation::activate() {
    is_active_ = true;
    if (plane_) {
        ncplane_set_fg_rgb8(plane_, 255, 255, 255); // White foreground
        ncplane_set_bg_rgb8(plane_, 0, 0, 0);     // Black background
    }
    time_since_last_trigger_ = trigger_cooldown_s_;
}

void RandomTextAnimation::deactivate() {
    is_active_ = false;
    if (plane_) {
        ncplane_erase(plane_); // Clear the plane when deactivated
    }
    active_lines_.clear();
}

void RandomTextAnimation::update(float delta_time,
                                 const AudioMetrics& metrics,
                                 const std::vector<float>& bands,
                                 float beat_strength) {
    if (!plane_ || !is_active_) return;

    (void)metrics;

    time_since_last_trigger_ += delta_time;

    float audio_value = beat_strength;
    if (trigger_band_index_ >= 0 && trigger_band_index_ < static_cast<int>(bands.size())) {
        audio_value = bands[static_cast<std::size_t>(trigger_band_index_)];
    }

    const bool beat_in_range = beat_strength >= trigger_beat_min_ && beat_strength <= trigger_beat_max_;
    if (beat_in_range && audio_value >= trigger_threshold_ && time_since_last_trigger_ >= trigger_cooldown_s_) {
        spawn_line();
        time_since_last_trigger_ = 0.0f;
    }

    const float word_interval = type_speed_words_per_s_ > 0.0f ? 1.0f / type_speed_words_per_s_ : 0.0f;

    for (auto& line : active_lines_) {
        if (!line.completed) {
            if (word_interval <= 0.0f) {
                line.current_word_index = line.words.size();
                line.completed = true;
                line.time_since_last_word = 0.0f;
            } else {
                line.time_since_last_word += delta_time;
                while (line.time_since_last_word >= word_interval && line.current_word_index < line.words.size()) {
                    line.time_since_last_word -= word_interval;
                    ++line.current_word_index;
                }
                if (line.current_word_index >= line.words.size()) {
                    line.current_word_index = line.words.size();
                    line.completed = true;
                    line.time_since_last_word = 0.0f;
                }
            }
        } else if (!line.fading_out) {
            line.display_elapsed += delta_time;
            if (line.display_elapsed >= display_duration_s_) {
                line.fading_out = true;
                line.fade_elapsed = 0.0f;
            }
        } else {
            line.fade_elapsed += delta_time;
        }
    }

    active_lines_.erase(
        std::remove_if(active_lines_.begin(),
                        active_lines_.end(),
                        [&](const DisplayedLine& line) {
                            if (!line.fading_out) {
                                return false;
                            }
                            if (fade_duration_s_ <= 0.0f) {
                                return true;
                            }
                            return line.fade_elapsed >= fade_duration_s_;
                        }),
        active_lines_.end());

    update_line_positions();
}

void RandomTextAnimation::render(notcurses* nc) {
    if (!plane_ || !is_active_) return;

    (void)nc;

    ncplane_erase(plane_);

    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(plane_, &plane_rows, &plane_cols);

    ncplane_set_bg_rgb8(plane_, 0, 0, 0);

    for (const auto& line : active_lines_) {
        if (line.words.empty()) {
            continue;
        }

        const std::size_t word_count = std::min(line.current_word_index, line.words.size());
        if (word_count == 0) {
            continue;
        }

        std::string rendered_text;
        rendered_text.reserve(line.text.size());
        for (std::size_t i = 0; i < word_count; ++i) {
            rendered_text.append(line.words[i]);
            if (i + 1 < word_count) {
                rendered_text.push_back(' ');
            }
        }

        float fade_factor = 1.0f;
        if (line.fading_out && fade_duration_s_ > 0.0f) {
            fade_factor = std::max(0.0f, 1.0f - (line.fade_elapsed / fade_duration_s_));
        }

        const unsigned char intensity = static_cast<unsigned char>(std::clamp(fade_factor, 0.0f, 1.0f) * 255.0f);
        ncplane_set_fg_rgb8(plane_, intensity, intensity, intensity);

        int y = std::clamp(line.y_pos, 0, static_cast<int>(plane_rows) - 1);
        int x = (static_cast<int>(plane_cols) - static_cast<int>(rendered_text.length())) / 2;
        if (x < 0) {
            x = 0;
        }

        ncplane_putstr_yx(plane_, y, x, rendered_text.c_str());
    }
}

void RandomTextAnimation::load_quotes() {
    quotes_.clear();

    auto trim = [](std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        return s;
    };

    auto try_load = [&](const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::string cleaned = trim(line);
            if (!cleaned.empty()) {
                quotes_.push_back(cleaned);
            }
        }
        return !quotes_.empty();
    };

    if (!try_load(text_file_path_) && text_file_path_ != "assets/dune.txt") {
        try_load("assets/dune.txt");
    }

    if (quotes_.empty()) {
        quotes_.push_back("Fear is the mind-killer.");
    }

    quote_dist_ = std::uniform_int_distribution<std::size_t>(0, quotes_.size() - 1);
}

std::string RandomTextAnimation::select_random_quote() {
    if (quotes_.empty()) {
        return {};
    }
    const std::size_t index = quote_dist_(rng_);
    return quotes_[index];
}

void RandomTextAnimation::spawn_line() {
    if (quotes_.empty()) {
        return;
    }

    if (static_cast<int>(active_lines_.size()) >= max_active_lines_) {
        active_lines_.erase(active_lines_.begin());
    }

    DisplayedLine line;
    line.text = select_random_quote();

    std::istringstream iss(line.text);
    std::string word;
    while (iss >> word) {
        line.words.push_back(word);
    }

    if (line.words.empty()) {
        return;
    }

    active_lines_.push_back(std::move(line));
    update_line_positions();
}

void RandomTextAnimation::update_line_positions() {
    if (!plane_) {
        return;
    }

    unsigned int plane_rows = 0;
    unsigned int plane_cols = 0;
    ncplane_dim_yx(plane_, &plane_rows, &plane_cols);
    (void)plane_cols;

    int current_y = static_cast<int>(plane_rows) - kBottomMargin;
    for (auto it = active_lines_.rbegin(); it != active_lines_.rend(); ++it) {
        it->y_pos = std::max(0, current_y);
        current_y -= kLineSpacing;
    }
}

} // namespace animations
} // namespace why