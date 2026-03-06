set(LUMIOS_SRC ${CMAKE_SOURCE_DIR}/lumios/src)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

set(IMGUIZMO_DIR ${ImGuizmo_SOURCE_DIR})
set(IMGUIZMO_SOURCES
    ${IMGUIZMO_DIR}/ImGuizmo.cpp
)

set(ENGINE_SOURCES
    ${LUMIOS_SRC}/core/log.cpp
    ${LUMIOS_SRC}/core/input.cpp
    ${LUMIOS_SRC}/platform/window.cpp
    ${LUMIOS_SRC}/assets/loader.cpp
    ${LUMIOS_SRC}/scene/scene_serializer.cpp
    ${LUMIOS_SRC}/scripting/script_manager.cpp
    ${LUMIOS_SRC}/physics/physics_world.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_init.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_swapchain.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_pipeline.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_buffer.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_descriptors.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_texture.cpp
    ${LUMIOS_SRC}/graphics/vulkan/vk_mem.cpp
    ${LUMIOS_SRC}/graphics/stb_impl.cpp
)

set(EDITOR_SOURCES
    src/main.cpp
    src/editor_app.cpp
    src/editor_renderer.cpp
    src/editor_panels.cpp
    src/game_window.cpp
)

add_executable(editor ${EDITOR_SOURCES} ${ENGINE_SOURCES} ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES})

target_include_directories(editor PRIVATE
    ${LUMIOS_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUIZMO_DIR}
    ${stb_SOURCE_DIR}
)

target_link_libraries(editor PRIVATE
    Vulkan::Vulkan
    VulkanMemoryAllocator
    glm::glm
    glfw
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)

target_compile_definitions(editor PRIVATE
    LUMIOS_BUILD
    LUMIOS_SHADER_DIR="${CMAKE_SOURCE_DIR}/lumios/src/shaders/binaries"
)
