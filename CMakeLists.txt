cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# CPM bootstrap
if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
  file(DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
    "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
  )
endif()
include("${CMAKE_BINARY_DIR}/cmake/CPM.cmake")

project ("lumios-engine" VERSION 1.0.0 LANGUAGES CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set output directories so DLL and EXE are in the same location
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For multi-config generators (like Visual Studio)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# Fetch Vulkan-Headers via CPM (header-only) without using git clone
CPMAddPackage(
  NAME VulkanHeaders
  URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.290.tar.gz
  DOWNLOAD_ONLY YES
)

# Create an interface target and alias Vulkan::Headers for use by subprojects
if (VulkanHeaders_ADDED)
  add_library(VulkanHeaders INTERFACE)
  target_include_directories(VulkanHeaders INTERFACE ${VulkanHeaders_SOURCE_DIR}/include)
  add_library(Vulkan::Headers ALIAS VulkanHeaders)
endif()

# Fetch tinygltf via CPM (header-only GLTF 2.0 library)
CPMAddPackage(
  NAME tinygltf
  GITHUB_REPOSITORY syoyo/tinygltf
  VERSION 2.9.3
  OPTIONS
    "TINYGLTF_HEADER_ONLY ON"
    "TINYGLTF_INSTALL ON"
    "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
    "TINYGLTF_BUILD_GL_EXAMPLES OFF"
    "TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF"
    "TINYGLTF_BUILD_BUILDER_EXAMPLE OFF"
)

# Create an interface target for tinygltf if added
if (tinygltf_ADDED)
  add_library(tinygltf_interface INTERFACE)
  target_include_directories(tinygltf_interface INTERFACE ${tinygltf_SOURCE_DIR})
  # tinygltf requires stb_image and nlohmann/json, which are bundled
  target_compile_definitions(tinygltf_interface INTERFACE 
    TINYGLTF_IMPLEMENTATION
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
  )
  add_library(tinygltf::tinygltf ALIAS tinygltf_interface)
endif()

# Fetch GLFW via CPM (window management and input handling)
# Using commit hash for reliability since version tags seem to have issues
CPMAddPackage(
  NAME glfw
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 45ce5ddd197d5c58f50fdd3296a5131c894e5527  # GLFW 3.3.8 stable
  OPTIONS
    "GLFW_BUILD_EXAMPLES OFF"
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_DOCS OFF"
    "GLFW_INSTALL ON"
)

# Fetch GLM via CPM (OpenGL Mathematics library)
CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 0.9.9.8
  OPTIONS
    "GLM_BUILD_TESTS OFF"
)

add_subdirectory ("lumios")
add_subdirectory ("application")
