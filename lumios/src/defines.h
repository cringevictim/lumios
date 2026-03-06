#pragma once

#ifdef _WIN32
    #ifdef LUMIOS_BUILD
        #define LUMIOS_API __declspec(dllexport)
    #else
        #define LUMIOS_API __declspec(dllimport)
    #endif
    #define LUMIOS_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define LUMIOS_API __attribute__((visibility("default")))
    #define LUMIOS_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define LUMIOS_API __attribute__((visibility("default")))
    #define LUMIOS_PLATFORM_MACOS
#else
    #define LUMIOS_API
#endif

#ifndef NDEBUG
    #define LUMIOS_DEBUG 1
#else
    #define LUMIOS_DEBUG 0
#endif

#define LUMIOS_VERSION_MAJOR 1
#define LUMIOS_VERSION_MINOR 0
#define LUMIOS_VERSION_PATCH 0
