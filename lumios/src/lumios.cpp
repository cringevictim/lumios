#include "lumios.h"

#include <print>

namespace lumios {

	LUMIOS_API int initialize() {
		std::println("Lumios Initialized");
		return 0;
	}

}