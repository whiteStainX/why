# Animation System Refactoring - Detailed Steps

This document outlines the detailed steps for refactoring the animation system to support DAG orchestration, Z-ordering, and a more modular design. Each step includes success criteria to ensure steady progress.

---

## Phase 1: Foundational Changes (Completed)

These steps have already been completed.

1.  **Rename `draw_grid` to `render_frame` (Orchestrator)**
    *   **Description**: Renamed the main rendering function to better reflect its role as an orchestrator.
    *   **Files Changed**: `src/renderer.h`, `src/renderer.cpp`, `src/main.cpp`.
    *   **Success Criteria**:
        *   `draw_grid` no longer exists in the codebase.
        *   `render_frame` is declared in `renderer.h`, defined in `renderer.cpp`, and called in `main.cpp`.
        *   Project builds successfully.

2.  **Create `src/animations` Directory**
    *   **Description**: Established a dedicated directory for animation implementations.
    *   **Files Changed**: New directory `src/animations`.
    *   **Success Criteria**:
        *   `src/animations` directory exists.

3.  **Define `Animation` Base Interface**
    *   **Description**: Created a pure virtual base class for all animations.
    *   **Files Changed**: `src/animations/animation.h`.
    *   **Success Criteria**:
        *   `src/animations/animation.h` defines `class Animation` with a pure virtual `render` method.
        *   Project builds successfully.

4.  **Move "Random Text" to `RandomTextAnimation`**
    *   **Description**: Encapsulated the existing random text rendering logic into a concrete `Animation` implementation.
    *   **Files Changed**: `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `src/renderer.cpp` (removed old logic).
    *   **Success Criteria**:
        *   `RandomTextAnimation` class exists and implements `Animation::render`.
        *   `renderer.cpp` no longer contains the random text generation logic.
        *   Project builds successfully.

5.  **Refactor `render_frame` to Use `Animation` Interface**
    *   **Description**: Modified the orchestrator to manage and call the active `Animation` instance.
    *   **Files Changed**: `src/renderer.h`, `src/renderer.cpp`, `src/main.cpp`.
    *   **Success Criteria**:
        *   `renderer.h` declares `set_active_animation`.
        *   `renderer.cpp` implements `set_active_animation` and uses `current_animation->render()` in `render_frame`.
        *   `main.cpp` calls `set_active_animation` with `RandomTextAnimation`.
        *   Project builds successfully.

6.  **Remove Grid-Related Parameters and Config**
    *   **Description**: Cleaned up `grid_rows`, `grid_cols`, and related configuration/UI elements as they are no longer universally applicable.
    *   **Files Changed**: `src/renderer.h`, `src/renderer.cpp`, `src/animations/animation.h`, `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `src/config.h`, `src/config.cpp`, `src/main.cpp`.
    *   **Success Criteria**:
        *   `grid_rows`, `grid_cols` parameters are removed from `render_frame` and `Animation::render` signatures.
        *   `GridConfig` and its usage are removed from `config.h` and `config.cpp`.
        *   `main.cpp` no longer declares or manipulates grid dimensions.
        *   Project builds successfully without warnings related to `grid_rows`/`grid_cols`.

7.  **Remove Sensitivity-Related Parameters and Config**
    *   **Description**: Cleaned up `sensitivity` and related configuration/UI elements.
    *   **Files Changed**: `src/renderer.h`, `src/renderer.cpp`, `src/animations/animation.h`, `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`, `src/config.h`, `src/config.cpp`, `src/main.cpp`.
    *   **Success Criteria**:
        *   `sensitivity` parameter is removed from `render_frame` and `Animation::render` signatures.
        *   `SensitivityConfig` and its usage are removed from `config.h` and `config.cpp`.
        *   `main.cpp` no longer declares or manipulates sensitivity.
        *   Project builds successfully without warnings related to `sensitivity`.

8.  **Update `CMakeLists.txt` for Animation Sources**
    *   **Description**: Modified CMake to automatically discover `.cpp` files in `src/animations`.
    *   **Files Changed**: `CMakeLists.txt`.
    *   **Success Criteria**:
        *   `CMakeLists.txt` uses `file(GLOB_RECURSE ...)` for animation sources.
        *   Project builds successfully.

---

## Phase 2: Implementing Animation Management and Z-Ordering

1.  **Enhance `Animation` Interface for Lifecycle and Z-Ordering**
    *   **Description**: Add methods to the `Animation` base class to support initialization, updates, state queries, and Z-index.
    *   **Files Changed**: `src/animations/animation.h`.
    *   **Success Criteria**:
        *   `Animation` interface includes:
            *   `virtual void init(notcurses* nc, const AppConfig& config) = 0;`
            *   `virtual void update(float delta_time, const AudioMetrics& metrics, const std::vector<float>& bands, float beat_strength) = 0;`
            *   `virtual void render(notcurses* nc) = 0;` (simplified, as update handles data)
            *   `virtual bool is_active() const = 0;`
            *   `virtual int get_z_index() const = 0;`
            *   `virtual ncplane* get_plane() const = 0;` (or similar mechanism for plane access)
        *   Project builds successfully (will require updating `RandomTextAnimation` to implement these).

2.  **Update `RandomTextAnimation` to Implement New Interface**
    *   **Description**: Adapt `RandomTextAnimation` to the enhanced `Animation` interface.
    *   **Files Changed**: `src/animations/random_text_animation.h`, `src/animations/random_text_animation.cpp`.
    *   **Success Criteria**:
        *   `RandomTextAnimation` correctly implements all new virtual methods from `Animation`.
        *   It creates and manages its own `ncplane` during `init`.
        *   Its `render` method draws to its own `ncplane`.
        *   Project builds successfully.

3.  **Create `AnimationManager` Class**
    *   **Description**: Implement a class to manage multiple `Animation` instances, their lifecycle, and Z-ordering.
    *   **Files Changed**: New files `src/animations/animation_manager.h`, `src/animations/animation_manager.cpp`.
    *   **Success Criteria**:
        *   `AnimationManager` class exists with methods to:
            *   Add animations (`add_animation(std::unique_ptr<Animation> animation)`).
            *   Update all active animations (`update_all(...)`).
            *   Render all active animations (`render_all(...)`).
            *   Manage Z-ordering of animation planes.
        *   Project builds successfully.

4.  **Integrate `AnimationManager` into `Renderer`**
    *   **Description**: Modify `render_frame` to use the `AnimationManager` to update and render animations.
    *   **Files Changed**: `src/renderer.h`, `src/renderer.cpp`.
    *   **Success Criteria**:
        *   `renderer.cpp` contains an instance of `AnimationManager`.
        *   `render_frame` calls `animation_manager.update_all()` and `animation_manager.render_all()`.
        *   `set_active_animation` is removed (or repurposed if we want to keep a single "active" animation concept).
        *   Project builds successfully.

5.  **Update `main.cpp` to Use `AnimationManager`**
    *   **Description**: Modify `main.cpp` to initialize the `AnimationManager` and add initial animations.
    *   **Files Changed**: `src/main.cpp`.
    *   **Success Criteria**:
        *   `main.cpp` creates an `AnimationManager` instance.
        *   `main.cpp` adds `RandomTextAnimation` (and potentially others) to the manager.
        *   `main.cpp` calls `render_frame` without directly setting an active animation.
        *   Project builds successfully.

---

## Phase 3: Advanced Features (Future)

1.  **Implement DAG Logic for Animation Triggers**
2.  **Add More Animation Types**
3.  **Implement Animation Configuration via `why.toml`**
4.  **Implement Animation Composition**
5.  **Event-Driven Animation Triggers**
