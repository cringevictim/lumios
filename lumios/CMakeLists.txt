cmake_minimum_required (VERSION 3.12)
project (lumios VERSION 1.0.0)

add_library(lumios SHARED
 "src/lumios.cpp"
 "src/defines.h" 
 "src/core/utils/logger.h"
 "src/core/utils/logger.cpp" 
 "src/core/memory/smart_resource.h")

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
