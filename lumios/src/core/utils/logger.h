#pragma once

#include <string_view>
#include <format>
#include <print>

namespace lumios::utils {
	void log_message(std::string_view message);

	template<typename... Args>
		requires (sizeof...(Args) > 0)
	inline void log_message(std::format_string<Args...> fmt, Args&&... args) {
		std::println("[LOG]: {}", std::format(fmt, std::forward<Args>(args)...));
	}
}