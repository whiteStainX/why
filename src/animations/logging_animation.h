#pragma once

#include <deque>
#include <string>
#include <vector>

#include <notcurses/notcurses.h>

#include "animation.h"
#include "../config.h"

namespace why {
namespace animations {

class LoggingAnimation : public Animation {
public:
    LoggingAnimation();
    ~LoggingAnimation() override;

    void init(notcurses* nc, const AppConfig& config) override;
    void update(float delta_time,
                const AudioMetrics& metrics,
                const std::vector<float>& bands,
                float beat_strength) override;
    void render(notcurses* nc) override;

    void activate() override;
    void deactivate() override;

    bool is_active() const override { return is_active_; }
    int get_z_index() const override { return z_index_; }
    ncplane* get_plane() const override { return plane_; }

private:
    void load_messages();
    void ensure_plane(notcurses* nc);
    void recalculate_content_geometry();
    void append_next_line();
    void redraw();
    void draw_border();
    void draw_logs();
    std::string truncate_for_width(const std::string& line) const;

    ncplane* plane_ = nullptr;
    int z_index_ = 0;
    bool is_active_ = true;

    bool show_border_ = true;
    bool loop_messages_ = true;
    int padding_y_ = 1;
    int padding_x_ = 2;

    int plane_rows_ = 16;
    int plane_cols_ = 60;
    int plane_origin_y_ = 0;
    int plane_origin_x_ = 0;

    int content_rows_ = 0;
    int content_cols_ = 0;
    int content_origin_y_ = 0;
    int content_origin_x_ = 0;

    std::vector<std::string> messages_;
    std::size_t next_message_index_ = 0;
    std::deque<std::string> visible_lines_;

    float line_interval_s_ = 0.4f;
    float time_since_last_line_ = 0.0f;
    bool needs_redraw_ = true;

    std::string messages_file_path_;
    std::string title_;
};

} // namespace animations
} // namespace why
