cmake_minimum_required(VERSION 3.20)
project(game_scripts)

set(CMAKE_CXX_STANDARD 20)

file(GLOB SCRIPT_SOURCES "${CMAKE_SOURCE_DIR}/assets/scripts/*.cpp")

add_library(game_scripts SHARED ${SCRIPT_SOURCES})

target_include_directories(game_scripts PRIVATE
    ${CMAKE_SOURCE_DIR}/lumios/src
)

target_link_libraries(game_scripts PRIVATE
    glm::glm
    EnTT::EnTT
    glfw
)

set_target_properties(game_scripts PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/assets/scripts"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/assets/scripts"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/assets/scripts"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/assets/scripts"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/assets/scripts"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/assets/scripts"
)
