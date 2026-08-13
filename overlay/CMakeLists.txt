cmake_minimum_required(VERSION 3.20)

project(scripted_gui_overlay LANGUAGES CXX)

add_executable(
    gui_indexed_map_make
    src/gui_indexed_map_make.cpp
    src/gui_interpreter.cpp
)

target_compile_features(
    gui_indexed_map_make
    PRIVATE cxx_std_17
)

if(APPLE)
    find_package(SDL2 CONFIG REQUIRED)

    add_executable(
        scripted_gui_host_macos
        src/scripted_gui_host_macos.cpp
        src/gui_host_macos.cpp
        src/gui_plugin_registry_macos.cpp
        src/gui_plugin_manifest_macos.cpp
        src/gui_builtin_plugins_macos.cpp
        src/declarative_gui_plugin_macos.cpp
        src/gui_data_provider.cpp
        src/gui_data_bridge.cpp
        src/gui_data_bridge_codec.cpp
        src/gui_file_data_provider.cpp
        src/gui_sequence_data_provider.cpp
        src/gui_memory_data_bridge.cpp
        src/gui_lua_bridge.cpp
        src/gui_localization.cpp
        src/gui_marker_layer_macos.cpp
        src/gui_declarative_data.cpp
        src/gui_control_renderer.cpp
        src/gui_render_queue.cpp
        src/gui_indexed_map_macos.cpp
        src/gui_interpreter.cpp
        src/gui_custom_widget.cpp
        src/gui_behavior.cpp
        src/gui_application_bus.cpp
        src/gui_data.cpp
        src/gui_action_bridge.cpp
        src/gui_runtime.cpp
        src/gui_tick.cpp
        src/gui_text_renderer_macos.cpp
        src/gui_texture_loader_macos.cpp
        src/gui_indexed_map_core.cpp
    )

    target_compile_features(
        scripted_gui_host_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        scripted_gui_host_macos
        PRIVATE
        SDL2::SDL2
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )

    add_executable(
        gui_plugin_manifest_probe_macos
        src/gui_plugin_manifest_probe_macos.cpp
        src/gui_plugin_registry_macos.cpp
        src/gui_plugin_manifest_macos.cpp
        src/gui_interpreter.cpp
    )

    target_compile_features(
        gui_plugin_manifest_probe_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        gui_plugin_manifest_probe_macos
        PRIVATE SDL2::SDL2
    )

    add_executable(
        declarative_gui_plugin_probe_macos
        src/declarative_gui_plugin_probe_macos.cpp
        src/declarative_gui_plugin_macos.cpp
        src/gui_data_provider.cpp
        src/gui_file_data_provider.cpp
        src/gui_declarative_data.cpp
        src/gui_plugin_registry_macos.cpp
        src/gui_interpreter.cpp
        src/gui_data.cpp
    )

    target_compile_features(
        declarative_gui_plugin_probe_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        declarative_gui_plugin_probe_macos
        PRIVATE SDL2::SDL2
    )

    add_executable(
        gui_indexed_map_probe_macos
        src/gui_indexed_map_probe_macos.cpp
        src/gui_indexed_map_macos.cpp
        src/gui_texture_loader_macos.cpp
        src/gui_interpreter.cpp
        src/gui_runtime.cpp
        src/gui_data.cpp
        src/gui_indexed_map_core.cpp
    )

    target_compile_features(
        gui_indexed_map_probe_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        gui_indexed_map_probe_macos
        PRIVATE
        SDL2::SDL2
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )

    add_executable(
        gui_marker_layer_probe_macos
        src/gui_marker_layer_probe_macos.cpp
        src/gui_marker_layer_macos.cpp
        src/gui_indexed_map_macos.cpp
        src/gui_indexed_map_core.cpp
        src/gui_texture_loader_macos.cpp
        src/gui_text_renderer_macos.cpp
        src/gui_control_renderer.cpp
        src/gui_interpreter.cpp
        src/gui_runtime.cpp
        src/gui_declarative_data.cpp
        src/gui_data.cpp
        src/gui_localization.cpp
    )

    target_compile_features(
        gui_marker_layer_probe_macos
        PRIVATE cxx_std_17
    )

    target_link_libraries(
        gui_marker_layer_probe_macos
        PRIVATE
        SDL2::SDL2
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )
endif()

add_executable(
    gui_sequence_data_provider_probe
    src/gui_sequence_data_provider_probe.cpp
    src/gui_sequence_data_provider.cpp
    src/gui_data_provider.cpp
    src/gui_declarative_data.cpp
    src/gui_interpreter.cpp
    src/gui_data.cpp
)

target_compile_features(
    gui_sequence_data_provider_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_interpreter_probe
    src/gui_interpreter_probe.cpp
    src/gui_interpreter.cpp
    src/gui_behavior.cpp
    src/gui_data.cpp
    src/gui_runtime.cpp
)

target_compile_features(
    gui_interpreter_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_declarative_data_probe
    src/gui_declarative_data_probe.cpp
    src/gui_declarative_data.cpp
    src/gui_interpreter.cpp
    src/gui_data.cpp
    src/gui_behavior.cpp
    src/gui_action_bridge.cpp
    src/gui_runtime.cpp
)

target_compile_features(
    gui_declarative_data_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_application_bus_probe
    src/gui_application_bus_probe.cpp
    src/gui_application_bus.cpp
)

target_compile_features(
    gui_application_bus_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_data_provider_probe
    src/gui_data_provider_probe.cpp
    src/gui_data_provider.cpp
    src/gui_data.cpp
)

target_compile_features(
    gui_data_provider_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_data_bridge_probe
    src/gui_data_bridge_probe.cpp
    src/gui_data_bridge.cpp
    src/gui_memory_data_bridge.cpp
    src/gui_data_provider.cpp
    src/gui_data.cpp
)

target_compile_features(
    gui_data_bridge_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_data_bridge_codec_probe
    src/gui_data_bridge_codec_probe.cpp
    src/gui_data_bridge_codec.cpp
    src/gui_data.cpp
)

target_compile_features(
    gui_data_bridge_codec_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_lua_bridge_probe
    src/gui_lua_bridge_probe.cpp
    src/gui_lua_bridge.cpp
    src/gui_data_bridge.cpp
    src/gui_data_provider.cpp
    src/gui_data.cpp
)

target_compile_features(
    gui_lua_bridge_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_render_queue_probe
    src/gui_render_queue_probe.cpp
    src/gui_render_queue.cpp
)

add_executable(
    gui_localization_probe
    src/gui_localization_probe.cpp
    src/gui_localization.cpp
)

target_compile_features(
    gui_localization_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_warmap_appointment_probe
    src/gui_warmap_appointment_probe.cpp
    src/gui_interpreter.cpp
    src/gui_declarative_data.cpp
    src/gui_data.cpp
    src/gui_behavior.cpp
    src/gui_action_bridge.cpp
    src/gui_runtime.cpp
)

target_compile_features(
    gui_warmap_appointment_probe
    PRIVATE cxx_std_17
)

target_compile_features(
    gui_render_queue_probe
    PRIVATE cxx_std_17
)
