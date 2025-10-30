#include "logging_animation.h"

#include <algorithm>
#include <fstream>

namespace why {
namespace animations {

namespace {
int clamp_dimension(int value, int min_value, int max_value) {
    return std::max(min_value, std::min(value, max_value));
}

int safe_origin(int requested, int plane_extent, int parent_extent) {
    if (parent_extent <= 0) {
        return 0;
    }
    if (plane_extent >= parent_extent) {
        return 0;
    }
    requested = std::max(0, requested);
    if (requested + plane_extent > parent_extent) {
        requested = parent_extent - plane_extent;
    }
    return requested;
}
} // namespace

LoggingAnimation::LoggingAnimation() = default;

LoggingAnimation::~LoggingAnimation() {
    if (plane_) {
        ncplane_destroy(plane_);
        plane_ = nullptr;
    }
}

void LoggingAnimation::init(notcurses* nc, const AppConfig& config) {
    if (!nc) {
        return;
    }

    const AnimationConfig* matched_config = nullptr;
    for (const auto& anim_config : config.animations) {
        if (anim_config.type == "Logging") {
            matched_config = &anim_config;
            break;
        }
    }

    if (matched_config) {
        z_index_ = matched_config->z_index;
        is_active_ = matched_config->initially_active;
        show_border_ = matched_config->log_show_border;
        loop_messages_ = matched_config->log_loop_messages;
        padding_y_ = std::max(0, matched_config->log_padding_y);
        padding_x_ = std::max(0, matched_config->log_padding_x);
        line_interval_s_ = std::max(0.0f, matched_config->log_line_interval_s);
        title_ = matched_config->log_title;
        if (!matched_config->text_file_path.empty()) {
            messages_file_path_ = matched_config->text_file_path;
        }
        if (matched_config->plane_rows) {
            plane_rows_ = std::max(3, *matched_config->plane_rows);
        }
        if (matched_config->plane_cols) {
            plane_cols_ = std::max(6, *matched_config->plane_cols);
        }
        if (matched_config->plane_y) {
            plane_origin_y_ = *matched_config->plane_y;
        }
        if (matched_config->plane_x) {
            plane_origin_x_ = *matched_config->plane_x;
        }
    }

    if (messages_file_path_.empty()) {
        messages_file_path_ = "assets/logging_animation.txt";
    }

    ensure_plane(nc);
    load_messages();
    recalculate_content_geometry();
    time_since_last_line_ = 0.0f;
    if (is_active_ && content_rows_ > 0) {
        append_next_line();
        time_since_last_line_ = 0.0f;
    }
    needs_redraw_ = true;
}

void LoggingAnimation::ensure_plane(notcurses* nc) {
    if (!nc) {
        return;
    }

    if (plane_) {
        ncplane_destroy(plane_);
        plane_ = nullptr;
    }

    ncplane* stdplane = notcurses_stdplane(nc);
    if (!stdplane) {
        return;
    }

    unsigned int parent_rows = 0;
    unsigned int parent_cols = 0;
    ncplane_dim_yx(stdplane, &parent_rows, &parent_cols);

    plane_rows_ = clamp_dimension(plane_rows_, 3, static_cast<int>(parent_rows));
    plane_cols_ = clamp_dimension(plane_cols_, 6, static_cast<int>(parent_cols));

    plane_origin_y_ = safe_origin(plane_origin_y_, plane_rows_, static_cast<int>(parent_rows));
    plane_origin_x_ = safe_origin(plane_origin_x_, plane_cols_, static_cast<int>(parent_cols));

    ncplane_options opts{};
    opts.y = plane_origin_y_;
    opts.x = plane_origin_x_;
    opts.rows = static_cast<unsigned>(plane_rows_);
    opts.cols = static_cast<unsigned>(plane_cols_);
    opts.userptr = nullptr;
    opts.name = "logging_animation";
    opts.resizecb = nullptr;
    opts.flags = 0u;

    plane_ = ncplane_create(stdplane, &opts);
    if (!plane_) {
        return;
    }

    ncplane_set_fg_rgb8(plane_, 120, 255, 120);
    ncplane_set_bg_rgb8(plane_, 0, 0, 0);
}

void LoggingAnimation::load_messages() {
    messages_.clear();

    std::ifstream file(messages_file_path_);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            messages_.push_back(line);
        }
    }

    if (messages_.empty()) {
        messages_.push_back("[log] No log messages configured.");
    }

    next_message_index_ = 0;
    visible_lines_.clear();
}

void LoggingAnimation::recalculate_content_geometry() {
    const int border = show_border_ ? 1 : 0;
    content_origin_y_ = border + padding_y_;
    content_origin_x_ = border + padding_x_;
    content_rows_ = plane_rows_ - 2 * border - 2 * padding_y_;
    content_cols_ = plane_cols_ - 2 * border - 2 * padding_x_;
    if (content_rows_ < 0) {
        content_rows_ = 0;
    }
    if (content_cols_ < 0) {
        content_cols_ = 0;
    }

    while (visible_lines_.size() > static_cast<std::size_t>(content_rows_)) {
        visible_lines_.pop_front();
    }
}

void LoggingAnimation::append_next_line() {
    if (content_rows_ <= 0 || messages_.empty()) {
        return;
    }

    if (next_message_index_ >= messages_.size()) {
        if (loop_messages_) {
            next_message_index_ = 0;
        } else {
            return;
        }
    }

    visible_lines_.push_back(messages_[next_message_index_]);
    ++next_message_index_;

    while (visible_lines_.size() > static_cast<std::size_t>(content_rows_)) {
        visible_lines_.pop_front();
    }

    needs_redraw_ = true;
}

void LoggingAnimation::update(float delta_time,
                              const AudioMetrics& metrics,
                              const std::vector<float>& bands,
                              float beat_strength) {
    (void)metrics;
    (void)bands;
    (void)beat_strength;

    if (!plane_ || !is_active_) {
        return;
    }

    if (line_interval_s_ <= 0.0f) {
        if (messages_.empty() || content_rows_ <= 0) {
            return;
        }
        std::size_t iterations = loop_messages_ ? messages_.size() : (messages_.size() - next_message_index_);
        for (std::size_t i = 0; i < iterations; ++i) {
            append_next_line();
            if (!loop_messages_ && next_message_index_ >= messages_.size()) {
                break;
            }
        }
        time_since_last_line_ = 0.0f;
        return;
    }

    time_since_last_line_ += delta_time;
    while (time_since_last_line_ >= line_interval_s_) {
        append_next_line();
        time_since_last_line_ -= line_interval_s_;
        if (!loop_messages_ && next_message_index_ >= messages_.size()) {
            time_since_last_line_ = 0.0f;
            break;
        }
    }
}

void LoggingAnimation::draw_border() {
    if (!show_border_ || !plane_ || plane_rows_ < 2 || plane_cols_ < 2) {
        return;
    }

    const int last_row = plane_rows_ - 1;
    const int last_col = plane_cols_ - 1;

    ncplane_putchar_yx(plane_, 0, 0, '+');
    ncplane_putchar_yx(plane_, 0, last_col, '+');
    ncplane_putchar_yx(plane_, last_row, 0, '+');
    ncplane_putchar_yx(plane_, last_row, last_col, '+');

    for (int x = 1; x < last_col; ++x) {
        ncplane_putchar_yx(plane_, 0, x, '-');
        ncplane_putchar_yx(plane_, last_row, x, '-');
    }

    for (int y = 1; y < last_row; ++y) {
        ncplane_putchar_yx(plane_, y, 0, '|');
        ncplane_putchar_yx(plane_, y, last_col, '|');
    }

    if (!title_.empty() && plane_cols_ > 4) {
        int max_title = plane_cols_ - 4;
        std::string clipped = title_.substr(0, static_cast<std::size_t>(max_title));
        int start_x = 2;
        for (std::size_t i = 0; i < clipped.size() && (start_x + static_cast<int>(i)) < last_col; ++i) {
            ncplane_putchar_yx(plane_, 0, start_x + static_cast<int>(i), clipped[i]);
        }
    }
}

std::string LoggingAnimation::truncate_for_width(const std::string& line) const {
    if (content_cols_ <= 0) {
        return std::string();
    }
    if (static_cast<int>(line.size()) <= content_cols_) {
        return line;
    }
    return line.substr(0, static_cast<std::size_t>(content_cols_));
}

void LoggingAnimation::draw_logs() {
    if (!plane_ || content_rows_ <= 0 || content_cols_ <= 0) {
        return;
    }

    const int max_rows = content_origin_y_ + content_rows_;
    std::string blank(static_cast<std::size_t>(content_cols_), ' ');

    int y = content_origin_y_;
    for (const auto& line : visible_lines_) {
        if (y >= max_rows) {
            break;
        }
        std::string truncated = truncate_for_width(line);
        if (static_cast<int>(truncated.size()) < content_cols_) {
            truncated.append(static_cast<std::size_t>(content_cols_ - static_cast<int>(truncated.size())), ' ');
        }
        ncplane_putstr_yx(plane_, y, content_origin_x_, truncated.c_str());
        ++y;
    }

    while (y < max_rows) {
        ncplane_putstr_yx(plane_, y, content_origin_x_, blank.c_str());
        ++y;
    }
}

void LoggingAnimation::redraw() {
    if (!plane_) {
        return;
    }

    ncplane_erase(plane_);
    draw_border();
    draw_logs();
    needs_redraw_ = false;
}

void LoggingAnimation::render(notcurses* nc) {
    (void)nc;
    if (!plane_ || !is_active_) {
        return;
    }

    if (needs_redraw_) {
        redraw();
    }
}

void LoggingAnimation::activate() {
    if (is_active_) {
        return;
    }

    is_active_ = true;
    visible_lines_.clear();
    next_message_index_ = 0;
    if (content_rows_ > 0) {
        append_next_line();
    }
    time_since_last_line_ = 0.0f;
    needs_redraw_ = true;
}

void LoggingAnimation::deactivate() {
    if (!is_active_) {
        return;
    }

    is_active_ = false;
    visible_lines_.clear();
    next_message_index_ = 0;
    if (plane_) {
        ncplane_erase(plane_);
    }
    needs_redraw_ = false;
}

} // namespace animations
} // namespace why
