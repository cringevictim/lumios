set(LUMIOS_SOURCES
    src/lumios.cpp
    src/core/log.cpp
    src/core/input.cpp
    src/platform/window.cpp
    src/assets/loader.cpp
    src/graphics/stb_impl.cpp
    src/graphics/vulkan/vk_mem.cpp
    src/graphics/vulkan/vk_init.cpp
    src/graphics/vulkan/vk_swapchain.cpp
    src/graphics/vulkan/vk_pipeline.cpp
    src/graphics/vulkan/vk_buffer.cpp
    src/graphics/vulkan/vk_descriptors.cpp
    src/graphics/vulkan/vk_texture.cpp
    src/graphics/vulkan/vk_renderer.cpp
)

add_library(lumios SHARED ${LUMIOS_SOURCES})

target_include_directories(lumios
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${stb_SOURCE_DIR}
)

target_link_libraries(lumios
    PUBLIC  glm::glm glfw EnTT::EnTT
    PRIVATE Vulkan::Vulkan VulkanMemoryAllocator
)

target_compile_definitions(lumios
    PRIVATE LUMIOS_BUILD
    PUBLIC  $<$<CONFIG:Debug>:LUMIOS_DEBUG=1>
)

# --- Shader compilation ---
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)

if(NOT GLSLC)
    message(WARNING "glslc not found - shaders will not be compiled")
else()
    set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
    set(SHADER_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/binaries")
    file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

    file(GLOB SHADER_SOURCES "${SHADER_SRC_DIR}/*.vert" "${SHADER_SRC_DIR}/*.frag" "${SHADER_SRC_DIR}/*.comp")

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPV_OUTPUT "${SHADER_BIN_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT  ${SPV_OUTPUT}
            COMMAND ${GLSLC} ${SHADER} -o ${SPV_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${SHADER_NAME}"
        )
        list(APPEND SPV_SHADERS ${SPV_OUTPUT})
    endforeach()

    add_custom_target(lumios_shaders ALL DEPENDS ${SPV_SHADERS})
    add_dependencies(lumios lumios_shaders)
endif()
