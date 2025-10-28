cmake_minimum_required(VERSION 3.16)
project(why LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# --- notcurses via pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES QUIET IMPORTED_TARGET notcurses)
if (NOT NOTCURSES_FOUND)
  pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses-core)
endif()

# --- sources ---
set(WHY_SOURCES
  src/main.cpp
  src/audio_engine.cpp
  src/config.cpp
  src/plugins.cpp
  src/renderer.cpp
  src/dsp.cpp
  src/animations/random_text_animation.cpp
  src/animations/bar_visual_animation.cpp
  src/animations/static_circle_animation.cpp
  external/kissfft/kiss_fft.c
)

file(GLOB_RECURSE ANIMATION_SOURCES "src/animations/*.cpp")

add_executable(why ${WHY_SOURCES} ${ANIMATION_SOURCES} why.toml)

# --- includes ---
target_include_directories(why PRIVATE
  src
  external/miniaudio
  external/kissfft
)

# --- link notcurses (and its transitive deps) ---
target_link_libraries(why PRIVATE PkgConfig::NOTCURSES)
