cmake_minimum_required(VERSION 3.20)

project(scripted_gui_overlay LANGUAGES CXX)

if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /EHsc)
endif()

include(CTest)

file(
    REAL_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    SCRIPTED_GUI_HOI3_MOD_ROOT_NATIVE
)

file(
    TO_CMAKE_PATH
    "${SCRIPTED_GUI_HOI3_MOD_ROOT_NATIVE}"
    SCRIPTED_GUI_HOI3_MOD_ROOT
)

configure_file(
    config/hoi3_scripted_gui_dev.mod.in
    "${CMAKE_BINARY_DIR}/hoi3_scripted_gui_dev.mod"
    @ONLY
)

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
        src/gui_plugin_registry.cpp
        src/gui_plugin_manifest.cpp
        src/gui_builtin_plugins.cpp
        src/declarative_gui_plugin.cpp
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
        src/gui_window_session.cpp
        src/gui_window_manager.cpp
        src/gui_persistence.cpp
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

if(WIN32)
    add_library(
        scripted_gui_overlay
        SHARED
        src/scripted_gui_overlay.def
        src/scripted_gui_overlay_dll.cpp
        src/gui_d3d9_hook.cpp
        src/gui_diagnostics.cpp
        src/gui_hoi3_lifecycle.cpp
        src/gui_host_d3d9.cpp
        src/gui_texture_loader_d3d9.cpp
        src/gui_text_renderer_d3d9.cpp
        src/gui_marker_layer_d3d9.cpp
        src/gui_indexed_map_d3d9.cpp
        src/gui_indexed_map_core.cpp
        src/gui_inprocess_application.cpp
        src/gui_window_session.cpp
        src/gui_window_manager.cpp
        src/gui_persistence.cpp
        src/gui_declarative_data.cpp
        src/gui_plugin_registry.cpp
        src/gui_plugin_manifest.cpp
        src/gui_builtin_plugins.cpp
        src/declarative_gui_plugin.cpp
        src/gui_data_provider.cpp
        src/gui_data_bridge.cpp
        src/gui_file_data_provider.cpp
        src/gui_sequence_data_provider.cpp
        src/gui_lua_bridge.cpp
        src/gui_lua51_hook.cpp
        src/gui_lua_native_binding.cpp
        src/gui_interpreter.cpp
        src/gui_behavior.cpp
        src/gui_action_bridge.cpp
        src/gui_runtime.cpp
        src/gui_data.cpp
        src/gui_application_bus.cpp
        src/gui_custom_widget.cpp
        src/gui_tick.cpp
        src/gui_localization.cpp
        src/gui_render_queue.cpp
    )

    target_compile_features(
        scripted_gui_overlay
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        scripted_gui_overlay
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            SCRIPTED_GUI_OVERLAY_EXPORTS
    )

    target_link_libraries(
        scripted_gui_overlay
        PRIVATE d3d9 gdiplus user32
    )

    add_executable(
        gui_host_d3d9_probe
        src/gui_host_d3d9_probe.cpp
        src/gui_host_d3d9.cpp
        src/gui_diagnostics.cpp
        src/gui_hoi3_lifecycle.cpp
        src/gui_texture_loader_d3d9.cpp
        src/gui_text_renderer_d3d9.cpp
        src/gui_marker_layer_d3d9.cpp
        src/gui_indexed_map_d3d9.cpp
        src/gui_indexed_map_core.cpp
        src/gui_inprocess_application.cpp
        src/gui_window_session.cpp
        src/gui_window_manager.cpp
        src/gui_persistence.cpp
        src/gui_declarative_data.cpp
        src/gui_plugin_registry.cpp
        src/gui_plugin_manifest.cpp
        src/gui_builtin_plugins.cpp
        src/declarative_gui_plugin.cpp
        src/gui_data_provider.cpp
        src/gui_data_bridge.cpp
        src/gui_file_data_provider.cpp
        src/gui_sequence_data_provider.cpp
        src/gui_lua_bridge.cpp
        src/gui_lua_native_binding.cpp
        src/gui_interpreter.cpp
        src/gui_behavior.cpp
        src/gui_action_bridge.cpp
        src/gui_runtime.cpp
        src/gui_data.cpp
        src/gui_application_bus.cpp
        src/gui_custom_widget.cpp
        src/gui_tick.cpp
        src/gui_localization.cpp
        src/gui_render_queue.cpp
    )

    target_compile_features(
        gui_host_d3d9_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        gui_host_d3d9_probe
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    )

    target_link_libraries(
        gui_host_d3d9_probe
        PRIVATE d3d9 gdiplus user32
    )

    add_executable(
        gui_lua51_native_probe
        src/gui_lua51_native_probe.cpp
        src/gui_lua51_hook.cpp
        src/gui_diagnostics.cpp
        src/gui_lua_native_binding.cpp
        src/gui_lua_bridge.cpp
        src/gui_data_bridge.cpp
        src/gui_data_provider.cpp
        src/gui_data.cpp
    )

    target_compile_features(
        gui_lua51_native_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        gui_lua51_native_probe
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    )

    add_executable(
        gui_hoi3_lifecycle_probe
        src/gui_hoi3_lifecycle_probe.cpp
        src/gui_hoi3_lifecycle.cpp
    )

    target_compile_features(
        gui_hoi3_lifecycle_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        gui_hoi3_lifecycle_probe
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    )

    add_executable(
        scripted_gui_injector
        src/scripted_gui_injector.cpp
    )

    target_compile_features(
        scripted_gui_injector
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        scripted_gui_injector
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    )
endif()

add_executable(
    gui_plugin_manifest_probe
    src/gui_plugin_manifest_probe_macos.cpp
    src/gui_plugin_registry.cpp
    src/gui_plugin_manifest.cpp
    src/gui_interpreter.cpp
)

target_compile_features(
    gui_plugin_manifest_probe
    PRIVATE cxx_std_17
)

add_executable(
    declarative_gui_plugin_probe
    src/declarative_gui_plugin_probe_macos.cpp
    src/declarative_gui_plugin.cpp
    src/gui_data_provider.cpp
    src/gui_file_data_provider.cpp
    src/gui_declarative_data.cpp
    src/gui_plugin_registry.cpp
    src/gui_interpreter.cpp
    src/gui_data.cpp
)

target_compile_features(
    declarative_gui_plugin_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_window_session_probe
    src/gui_window_session_probe.cpp
    src/gui_window_session.cpp
    src/gui_persistence.cpp
    src/gui_declarative_data.cpp
    src/gui_interpreter.cpp
    src/gui_behavior.cpp
    src/gui_action_bridge.cpp
    src/gui_runtime.cpp
    src/gui_data.cpp
    src/gui_application_bus.cpp
    src/gui_custom_widget.cpp
    src/gui_tick.cpp
)

target_compile_features(
    gui_window_session_probe
    PRIVATE cxx_std_17
)

add_executable(
    gui_inprocess_application_probe
    src/gui_inprocess_application_probe.cpp
    src/gui_inprocess_application.cpp
    src/gui_window_session.cpp
    src/gui_persistence.cpp
    src/gui_declarative_data.cpp
    src/gui_plugin_registry.cpp
    src/gui_plugin_manifest.cpp
    src/gui_builtin_plugins.cpp
    src/declarative_gui_plugin.cpp
    src/gui_data_provider.cpp
    src/gui_data_bridge.cpp
    src/gui_file_data_provider.cpp
    src/gui_sequence_data_provider.cpp
    src/gui_lua_bridge.cpp
    src/gui_interpreter.cpp
    src/gui_behavior.cpp
    src/gui_action_bridge.cpp
    src/gui_runtime.cpp
    src/gui_data.cpp
    src/gui_application_bus.cpp
    src/gui_custom_widget.cpp
    src/gui_tick.cpp
)

target_compile_features(
    gui_inprocess_application_probe
    PRIVATE cxx_std_17
)

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

add_executable(
    gui_window_manager_probe
    src/gui_window_manager_probe.cpp
    src/gui_window_manager.cpp
)

add_executable(
    gui_persistence_probe
    src/gui_persistence_probe.cpp
    src/gui_persistence.cpp
)

target_compile_features(
    gui_persistence_probe
    PRIVATE cxx_std_17
)

target_compile_features(
    gui_window_manager_probe
    PRIVATE cxx_std_17
)

if(BUILD_TESTING)
    set(
        SCRIPTED_GUI_PROBE_TARGETS
        gui_sequence_data_provider_probe
        gui_interpreter_probe
        gui_declarative_data_probe
        gui_application_bus_probe
        gui_window_manager_probe
        gui_persistence_probe
        gui_data_provider_probe
        gui_data_bridge_probe
        gui_data_bridge_codec_probe
        gui_lua_bridge_probe
        gui_render_queue_probe
        gui_localization_probe
        gui_warmap_appointment_probe
        gui_plugin_manifest_probe
        declarative_gui_plugin_probe
        gui_window_session_probe
        gui_inprocess_application_probe
    )

    foreach(probe_target IN LISTS SCRIPTED_GUI_PROBE_TARGETS)
        add_test(NAME ${probe_target} COMMAND ${probe_target})
        set_tests_properties(
            ${probe_target}
            PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        )
    endforeach()

    if(WIN32)
        add_test(
            NAME gui_hoi3_lifecycle_probe
            COMMAND gui_hoi3_lifecycle_probe
        )
        set_tests_properties(
            gui_hoi3_lifecycle_probe
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        )
        add_test(NAME gui_host_d3d9_probe COMMAND gui_host_d3d9_probe)
        set_tests_properties(
            gui_host_d3d9_probe
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
                SKIP_RETURN_CODE 77
        )
    endif()
endif()
