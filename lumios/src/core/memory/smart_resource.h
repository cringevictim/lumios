#pragma once

#include <memory>
#include <utility>

namespace lumios::memory {
	template<typename T>
	class SmartResource {
	public:
		SmartResource() = default;
		SmartResource(const T& value) : m_ResourcePtr(std::make_shared<T>(value)) {}
		SmartResource(T&& value) : m_ResourcePtr(std::make_shared<T>(std::move(value))) {}

		template<typename... Args>
		static SmartResource in_place(Args&&... args) {
			SmartResource r;
			r.m_ResourcePtr = std::make_shared<T>(std::forward<Args>(args)...);
			return r;
		}

		SmartResource(const SmartResource&) = default;
		SmartResource(SmartResource&&) noexcept = default;
		SmartResource& operator=(const SmartResource&) = default;
		SmartResource& operator=(SmartResource&&) noexcept = default;

		SmartResource& operator=(const T& value) {
			m_ResourcePtr = std::make_shared<T>(value);
			return *this;
		}
		SmartResource& operator=(T&& value) {
			m_ResourcePtr = std::make_shared<T>(std::move(value));
			return *this;
		}

		virtual ~SmartResource() = default;

		T* operator->() { return m_ResourcePtr.get(); }
		const T* operator->() const { return m_ResourcePtr.get(); }
		T& operator*() { return *m_ResourcePtr; }
		const T& operator*() const { return *m_ResourcePtr; }

		T* get() noexcept { return m_ResourcePtr.get(); }
		const T* get() const noexcept { return m_ResourcePtr.get(); }

		explicit operator bool() const noexcept { return static_cast<bool>(m_ResourcePtr); }
		bool has_value() const noexcept { return static_cast<bool>(m_ResourcePtr); }

		void reset() noexcept { m_ResourcePtr.reset(); }

		template<typename... Args>
		T& emplace(Args&&... args) {
			m_ResourcePtr = std::make_shared<T>(std::forward<Args>(args)...);
			return *m_ResourcePtr;
		}

	private:
		std::shared_ptr<T> m_ResourcePtr = nullptr;
	};
}