#include "logger.h"

namespace lumios::utils {
	void log_message(std::string_view message) {
		std::println("[LOG]: {}", message);
	}
}