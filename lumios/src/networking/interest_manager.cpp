#include "interest_manager.h"
#include <cmath>

namespace lumios::net {

void InterestManager::update_entity(EntityNetID id, const glm::vec3& position) {
    auto old_it = entity_positions_.find(id);
    if (old_it != entity_positions_.end()) {
        Cell old_cell = to_cell(old_it->second);
        spatial_grid_[old_cell].erase(id);
    }

    entity_positions_[id] = position;
    Cell cell = to_cell(position);
    spatial_grid_[cell].insert(id);
}

void InterestManager::remove_entity(EntityNetID id) {
    auto it = entity_positions_.find(id);
    if (it != entity_positions_.end()) {
        Cell cell = to_cell(it->second);
        spatial_grid_[cell].erase(id);
        entity_positions_.erase(it);
    }
}

void InterestManager::update_client(ClientID id, const glm::vec3& position) {
    client_positions_[id] = position;
}

void InterestManager::remove_client(ClientID id) {
    client_positions_.erase(id);
}

std::vector<EntityNetID> InterestManager::get_visible_entities(ClientID client) const {
    std::vector<EntityNetID> result;
    auto it = client_positions_.find(client);
    if (it == client_positions_.end()) return result;

    const glm::vec3& client_pos = it->second;
    float radius_sq = interest_radius_ * interest_radius_;
    int cell_range = static_cast<int>(std::ceil(interest_radius_ / cell_size_));
    Cell center = to_cell(client_pos);

    for (int dx = -cell_range; dx <= cell_range; dx++) {
        for (int dy = -cell_range; dy <= cell_range; dy++) {
            for (int dz = -cell_range; dz <= cell_range; dz++) {
                Cell check{center.x + dx, center.y + dy, center.z + dz};
                auto grid_it = spatial_grid_.find(check);
                if (grid_it == spatial_grid_.end()) continue;

                for (EntityNetID eid : grid_it->second) {
                    auto pos_it = entity_positions_.find(eid);
                    if (pos_it == entity_positions_.end()) continue;
                    glm::vec3 diff = pos_it->second - client_pos;
                    if (glm::dot(diff, diff) <= radius_sq) {
                        result.push_back(eid);
                    }
                }
            }
        }
    }

    return result;
}

} // namespace lumios::net
