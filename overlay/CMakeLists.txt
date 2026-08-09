cmake_minimum_required(VERSION 3.20)

project(war_map_preview LANGUAGES CXX)

add_executable(
    chian_map_make
    src/chian_map_make.cpp
)

target_compile_features(
    chian_map_make
    PRIVATE cxx_std_17
)

if(WIN32)
    add_executable(
        war_map_preview
        src/war_map_preview.cpp
        src/gui_interpreter.cpp
        src/region_color_core.cpp
        src/war_map_state.cpp
    )

    target_compile_features(
        war_map_preview
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        war_map_preview
        PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_link_libraries(
        war_map_preview
        PRIVATE
        d3d9
        d3dx9
        user32
    )
endif()

if(APPLE)
    find_package(SDL2 CONFIG REQUIRED)

    add_executable(
        war_map_preview_macos
        src/war_map_preview_macos.cpp
        src/gui_interpreter.cpp
        src/region_color_core.cpp
        src/war_map_state.cpp
    )

    target_compile_features(
        war_map_preview_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        war_map_preview_macos
        PRIVATE
        SDL2::SDL2
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )
endif()

add_executable(
    gui_interpreter_probe
    src/gui_interpreter_probe.cpp
    src/gui_interpreter.cpp
)

target_compile_features(
    gui_interpreter_probe
    PRIVATE cxx_std_17
)
