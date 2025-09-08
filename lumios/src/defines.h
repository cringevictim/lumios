#pragma once

#ifdef _WIN32
#ifdef LUMIOS_BUILD
#define LUMIOS_API __declspec(dllexport)
#else
#define LUMIOS_API __declspec(dllimport)
#endif
#else
#define LUMIOS_API
#endif

