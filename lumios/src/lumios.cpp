#include "lumios.h"

#include "core/utils/logger.h"
#include "core/memory/smart_resource.h"	

#include <print>

namespace lumios {

	struct ExampleStruct {
		int a;
		float b;
	};

	LUMIOS_API int initialize() {
		// temp
		

		memory::SmartResource<ExampleStruct> res1 = ExampleStruct{1, 1.5};
		utils::log_message("SmartResource res1 created with values: int {}, float {}", res1->a, res1->b);

		utils::log_message("Lumios initialized successfully.");
		return 0;
	}

}