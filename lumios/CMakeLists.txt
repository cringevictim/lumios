cmake_minimum_required (VERSION 3.12)
project (lumios VERSION 1.0.0)

add_library(lumios SHARED
 "src/lumios.cpp"
 "src/lumios.h"
 "src/defines.h" 
 "src/core/utils/logger.h"
 "src/core/utils/logger.cpp" 
 "src/core/memory/smart_resource.h"
 
 # Graphics system
 "src/graphics/config/graphics_config.h"
 "src/graphics/config/graphics_config.cpp"
 "src/graphics/backend/graphics_backend.h"
 "src/graphics/backend/graphics_backend.cpp"
 "src/graphics/backend/vulkan/vulkan_backend.h"
 "src/graphics/backend/vulkan/vulkan_backend.cpp"
 "src/graphics/backend/directx12/dx12_backend.h"
 "src/graphics/backend/directx12/dx12_backend.cpp"
 "src/graphics/frontend/graphics_frontend.h"
 "src/graphics/frontend/graphics_frontend.cpp" "src/graphics/backend/vulkan/vulkan_utils/vulkan_instance.cpp" "src/graphics/backend/vulkan/vulkan_utils/vulkan_instance.h")

target_include_directories(lumios PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(lumios PRIVATE LUMIOS_BUILD)

# Set library properties
set_target_properties(lumios PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "lumios"
    CLEAN_DIRECT_OUTPUT 1
)

# Install rules
install(TARGETS lumios
    EXPORT lumiosTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES src/lumios.h
    DESTINATION include
)

# Install tinygltf headers if available
if(TARGET tinygltf::tinygltf AND tinygltf_ADDED)
    install(FILES ${tinygltf_SOURCE_DIR}/tiny_gltf.h
        DESTINATION include
    )
endif()

# Link header-only Vulkan-Headers if available (from root CPM)
if(TARGET Vulkan::Headers)
  target_link_libraries(lumios PUBLIC Vulkan::Headers)
endif()

# Link tinygltf if available (from root CPM)
if(TARGET tinygltf::tinygltf)
  target_link_libraries(lumios PUBLIC tinygltf::tinygltf)
endif()

# Link GLFW if available (from root CPM)
if(TARGET glfw)
  target_link_libraries(lumios PUBLIC glfw)
endif()

# Link GLM if available (from root CPM)
if(TARGET glm::glm)
  target_link_libraries(lumios PUBLIC glm::glm)
endif()

# Optional fallback to system Vulkan SDK for loader (no volk requested)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_link_libraries(lumios PUBLIC Vulkan::Vulkan)
endif()