#include "lumios.h"

#include "core/utils/logger.h"
#include "core/memory/smart_resource.h"	

#include <print>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>


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
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::cout << extensionCount << " extensions supported\n";

		glm::mat4 matrix;
		glm::vec4 vec;
		auto test = matrix * vec;

		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}


		glfwDestroyWindow(window);

		glfwTerminate();

		return 0;
	}

}