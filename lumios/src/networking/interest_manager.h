#pragma once

#include "net_types.h"
#include <unordered_map>
#include <unordered_set>

namespace lumios::net {

class InterestManager {
public:
    void set_cell_size(float size) { cell_size_ = size; }

    void update_entity(EntityNetID id, const glm::vec3& position);
    void remove_entity(EntityNetID id);

    void update_client(ClientID id, const glm::vec3& position);
    void remove_client(ClientID id);

    std::vector<EntityNetID> get_visible_entities(ClientID client) const;

    void set_interest_radius(float radius) { interest_radius_ = radius; }

private:
    struct Cell {
        i32 x, y, z;
        bool operator==(const Cell& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct CellHash {
        size_t operator()(const Cell& c) const {
            size_t h = 0;
            h ^= std::hash<i32>()(c.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<i32>()(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<i32>()(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    Cell to_cell(const glm::vec3& pos) const {
        return {
            static_cast<i32>(std::floor(pos.x / cell_size_)),
            static_cast<i32>(std::floor(pos.y / cell_size_)),
            static_cast<i32>(std::floor(pos.z / cell_size_))
        };
    }

    float cell_size_       = 50.0f;
    float interest_radius_ = 200.0f;

    std::unordered_map<EntityNetID, glm::vec3> entity_positions_;
    std::unordered_map<ClientID, glm::vec3>    client_positions_;
    std::unordered_map<Cell, std::unordered_set<EntityNetID>, CellHash> spatial_grid_;
};

} // namespace lumios::net
