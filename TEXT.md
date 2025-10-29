# Enhancing `RandomTextAnimation` - Detailed Steps

This document outlines the detailed steps to transform `RandomTextAnimation` into a dynamic, audio-reactive quote display with a typewriter effect, using quotes from `assets/dune.txt`.

---

## Phase 4: Enhanced `RandomTextAnimation`

### Step 1: Load Quotes from File

*   **Description**: Modify `RandomTextAnimation` to load all quotes from `assets/dune.txt` into an internal data structure during its initialization.
*   **Files Changed**: `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`.
*   **Success Criteria**:
    *   `RandomTextAnimation` has a `std::vector<std::string>` member to store the quotes.
    *   `RandomTextAnimation::init` opens and reads `assets/dune.txt`, populating the vector.
    *   Error handling is in place if the file cannot be opened.
    *   Project builds successfully.
    *   (Optional debug logging) Confirm quotes are loaded correctly.

### Step 2: Define `DisplayedLine` Structure

*   **Description**: Create a helper struct or class within `RandomTextAnimation` (or as a private nested class) to manage the state of a single line being displayed. This will include the quote text, its current display progress, position, and timers.
*   **Files Changed**: `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`.
*   **Success Criteria**:
    *   A `struct DisplayedLine` (or similar) is defined with members like:
        *   `std::string text;`
        *   `std::vector<std::string> words;` (pre-parsed words for typewriter effect)
        *   `int current_word_index;`
        *   `float display_timer;`
        *   `float fade_timer;`
        *   `int y_pos;`
        *   `int x_pos;`
        *   `bool fading_out;`
    *   `RandomTextAnimation` manages a `std::list<DisplayedLine>` or `std::vector<DisplayedLine>`.
    *   Project builds successfully.

### Step 3: Implement Audio-Triggered New Line Display

*   **Description**: In `RandomTextAnimation::update`, use audio metrics (specifically `beat_strength` or a band energy threshold) to trigger the selection and display of a new random quote.
*   **Files Changed**: `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `why.toml` (for new config options).
*   **Success Criteria**:
    *   `RandomTextAnimation` has a member to track the last time a new line was triggered to prevent rapid-fire display.
    *   `RandomTextAnimation::update` checks `beat_strength` (or other audio condition) against a configurable threshold.
    *   If triggered, a random quote is selected from the loaded quotes, parsed into words, and a new `DisplayedLine` is added to the active list.
    *   `why.toml` includes new configuration options for `RandomTextAnimation` such as `trigger_cooldown_s`.
    *   Project builds successfully.
    *   When running, new lines of text appear in response to audio beats/energy.

### Step 4: Implement Typewriter Effect and Persistence

*   **Description**: Manage the word-by-word display of each `DisplayedLine` and its lifecycle (typing, full display, fading out).
*   **Files Changed**: `src/animations/random_text_animation.cpp`.
*   **Success Criteria**:
    *   `RandomTextAnimation::update` iterates through active `DisplayedLine`s.
    *   For each line, it advances `current_word_index` based on `delta_time` and a configurable `type_speed`.
    *   Once `current_word_index` reaches the end, a `display_timer` starts.
    *   After `display_timer` expires, `fading_out` flag is set, and a `fade_timer` starts.
    *   Lines are removed from the active list when `fade_timer` expires.
    *   `why.toml` includes new configuration options for `RandomTextAnimation` such as `type_speed_words_per_s`, `display_duration_s`, `fade_duration_s`.
    *   Project builds successfully.
    *   Text appears word by word, persists, and then disappears.

### Step 5: Render Multiple Lines with Positioning

*   **Description**: Modify `RandomTextAnimation::render` to draw all active `DisplayedLine`s, handling their vertical positioning and only rendering the currently typed words.
*   **Files Changed**: `src/animations/random_text_animation.cpp`.
*   **Success Criteria**:
    *   `RandomTextAnimation::render` iterates through the `std::list<DisplayedLine>`.
    *   Each line is drawn at its `y_pos` and `x_pos`.
    *   Only words up to `current_word_index` are drawn for each line.
    *   Vertical spacing is managed to prevent lines from overlapping (e.g., new lines appear at the bottom and push older lines up, or simply appear at random available positions).
    *   (Optional) Implement color fading during the `fade_timer` phase.
    *   Project builds successfully.
    *   Multiple lines of text are visible, typing out and disappearing.

### Step 6: Update `why.toml` with New Configuration Options

*   **Description**: Add all new configuration options for `RandomTextAnimation` to `why.toml` and ensure `config.cpp` parses them.
*   **Files Changed**: `why.toml`, `src/config.h`, `src/config.cpp`.
*   **Success Criteria**:
    *   `why.toml` contains `random_text` specific settings.
    *   `AnimationConfig` in `config.h` includes members for these settings.
    *   `config.cpp` parses these settings into `AnimationConfig`.
    *   Project builds successfully.

---
