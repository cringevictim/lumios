#pragma once

#include <entt/entt.hpp>
#include "components.h"

namespace lumios {

class Scene {
    entt::registry registry_;

public:
    entt::entity create_entity(const std::string& name = "") {
        auto e = registry_.create();
        registry_.emplace<Transform>(e);
        if (!name.empty()) registry_.emplace<NameComponent>(e, name);
        return e;
    }

    void destroy_entity(entt::entity e) {
        registry_.destroy(e);
    }

    template<typename T, typename... Args>
    T& add(entt::entity e, Args&&... args) {
        return registry_.emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T& get(entt::entity e) { return registry_.get<T>(e); }

    template<typename T>
    const T& get(entt::entity e) const { return registry_.get<T>(e); }

    template<typename T>
    bool has(entt::entity e) const { return registry_.all_of<T>(e); }

    template<typename... T>
    auto view() { return registry_.view<T...>(); }

    template<typename... T>
    auto view() const { return registry_.view<T...>(); }

    entt::registry&       registry()       { return registry_; }
    const entt::registry& registry() const { return registry_; }

    void clear() { registry_.clear(); }
};

} // namespace lumios
