#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace lumios {

struct WindowResizeEvent { uint32_t width, height; };
struct KeyEvent          { int key, scancode, action, mods; };
struct MouseMoveEvent    { double x, y; };
struct MouseButtonEvent  { int button, action, mods; };
struct ScrollEvent       { double x_offset, y_offset; };

class EventBus {
    struct IHandler { virtual ~IHandler() = default; };

    template<typename E>
    struct Handler : IHandler {
        std::function<void(const E&)> fn;
        Handler(std::function<void(const E&)> f) : fn(std::move(f)) {}
    };

    std::unordered_map<std::type_index, std::vector<std::unique_ptr<IHandler>>> handlers_;

public:
    template<typename E>
    void subscribe(std::function<void(const E&)> callback) {
        handlers_[typeid(E)].push_back(std::make_unique<Handler<E>>(std::move(callback)));
    }

    template<typename E>
    void emit(const E& event) {
        auto it = handlers_.find(typeid(E));
        if (it == handlers_.end()) return;
        for (auto& h : it->second) {
            static_cast<Handler<E>*>(h.get())->fn(event);
        }
    }
};

} // namespace lumios
