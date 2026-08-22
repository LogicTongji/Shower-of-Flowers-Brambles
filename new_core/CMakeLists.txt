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

if(WIN32)
    add_library(
        scripted_gui_overlay
        SHARED
        src/scripted_gui_overlay.def
        src/scripted_gui_overlay_dll.cpp
        src/core_hook_registry.cpp
        src/core_lifecycle.cpp
        src/core_module_registry.cpp
        src/core_runtime.cpp
        src/script_gui_core_module.cpp
        src/leader_capture_core_module.cpp
        src/leader_capture_engine.cpp
        src/new_core_handshake.cpp
        src/gui_d3d9_hook.cpp
        src/gui_diagnostics.cpp
        src/hoi3_lifecycle.cpp
        src/gui_host_d3d9.cpp
        src/gui_custom_widget_d3d9.cpp
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

    set_target_properties(
        scripted_gui_overlay
        PROPERTIES OUTPUT_NAME hoi3_new_core
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
        src/gui_custom_widget_d3d9.cpp
        src/gui_diagnostics.cpp
        src/hoi3_lifecycle.cpp
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
		src/gui_declarative_data.cpp
		src/gui_interpreter.cpp
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
        hoi3_lifecycle_probe
        src/hoi3_lifecycle_probe.cpp
        src/hoi3_lifecycle.cpp
    )

    target_compile_features(
        hoi3_lifecycle_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        hoi3_lifecycle_probe
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

    add_executable(
        hoi3_new_core_launcher
        WIN32
        src/new_core_launcher.cpp
        src/new_core_launcher_core.cpp
        src/new_core_handshake.cpp
    )

    target_compile_features(
        hoi3_new_core_launcher
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        hoi3_new_core_launcher
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
    )

    target_link_libraries(
        hoi3_new_core_launcher
        PRIVATE comctl32 comdlg32 shell32 ole32
    )

    add_executable(
        new_core_launcher_probe
        src/new_core_launcher_probe.cpp
        src/new_core_launcher_core.cpp
        src/new_core_handshake.cpp
    )

    target_compile_features(
        new_core_launcher_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        new_core_launcher_probe
        PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
    )
endif()

add_executable(
    gui_plugin_manifest_probe
    src/gui_plugin_manifest_probe.cpp
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
    src/declarative_gui_plugin_probe.cpp
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
	src/gui_declarative_data.cpp
	src/gui_interpreter.cpp
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
	src/gui_declarative_data.cpp
	src/gui_interpreter.cpp
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
    gui_custom_widget_probe
    src/gui_custom_widget_probe.cpp
    src/gui_custom_widget.cpp
)

target_compile_features(
    gui_custom_widget_probe
    PRIVATE cxx_std_17
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

add_executable(
    core_infrastructure_probe
    src/core_infrastructure_probe.cpp
    src/core_hook_registry.cpp
    src/core_lifecycle.cpp
    src/core_module_registry.cpp
)

target_compile_features(
    core_infrastructure_probe
    PRIVATE cxx_std_17
)

if(WIN32)
    add_executable(
        leader_capture_core_module_probe
        src/leader_capture_core_module_probe.cpp
        src/leader_capture_core_module.cpp
        src/core_hook_registry.cpp
        src/core_lifecycle.cpp
        src/leader_capture_engine.cpp
    )

    target_compile_features(
        leader_capture_core_module_probe
        PRIVATE cxx_std_17
    )

    target_compile_definitions(
        leader_capture_core_module_probe
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
endif()

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
        gui_custom_widget_probe
        gui_localization_probe
        gui_warmap_appointment_probe
        gui_plugin_manifest_probe
        declarative_gui_plugin_probe
        gui_window_session_probe
        gui_inprocess_application_probe
        core_infrastructure_probe
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
            NAME new_core_launcher_probe
            COMMAND new_core_launcher_probe
                "${CMAKE_CURRENT_SOURCE_DIR}/.."
                "$<TARGET_FILE:hoi3_new_core_launcher>"
                "$<TARGET_FILE:scripted_gui_overlay>"
                "${CMAKE_CURRENT_BINARY_DIR}/new_core_launcher_probe.ini"
        )
        set_tests_properties(
            new_core_launcher_probe
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        )
        add_test(
            NAME leader_capture_core_module_probe
            COMMAND leader_capture_core_module_probe
        )
        set_tests_properties(
            leader_capture_core_module_probe
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        )
        add_test(
            NAME hoi3_lifecycle_probe
            COMMAND hoi3_lifecycle_probe
        )
        set_tests_properties(
            hoi3_lifecycle_probe
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
