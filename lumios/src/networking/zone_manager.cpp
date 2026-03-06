#include "zone_manager.h"

namespace lumios::net {

void ZoneManager::add_zone(const ZoneConfig& config) {
    zones_[config.id] = config;
}

void ZoneManager::remove_zone(ZoneID id) {
    zones_.erase(id);
    std::erase_if(entity_zones_, [id](const auto& pair) { return pair.second == id; });
}

ZoneID ZoneManager::get_zone_for_position(const glm::vec3& position) const {
    for (auto& [id, zone] : zones_) {
        if (position.x >= zone.bounds_min.x && position.x <= zone.bounds_max.x &&
            position.y >= zone.bounds_min.y && position.y <= zone.bounds_max.y &&
            position.z >= zone.bounds_min.z && position.z <= zone.bounds_max.z) {
            return id;
        }
    }
    return INVALID_ZONE;
}

bool ZoneManager::should_transfer(EntityNetID entity, const glm::vec3& new_position) const {
    auto it = entity_zones_.find(entity);
    if (it == entity_zones_.end()) return false;

    auto zone_it = zones_.find(it->second);
    if (zone_it == zones_.end()) return false;

    const auto& zone = zone_it->second;
    return new_position.x < zone.bounds_min.x - boundary_margin_ ||
           new_position.x > zone.bounds_max.x + boundary_margin_ ||
           new_position.y < zone.bounds_min.y - boundary_margin_ ||
           new_position.y > zone.bounds_max.y + boundary_margin_ ||
           new_position.z < zone.bounds_min.z - boundary_margin_ ||
           new_position.z > zone.bounds_max.z + boundary_margin_;
}

void ZoneManager::register_entity(EntityNetID entity, ZoneID zone) {
    entity_zones_[entity] = zone;
}

void ZoneManager::unregister_entity(EntityNetID entity) {
    entity_zones_.erase(entity);
}

std::vector<ZoneManager::TransferRequest> ZoneManager::process_transfers(
    const std::unordered_map<EntityNetID, glm::vec3>& entity_positions) {

    std::vector<TransferRequest> transfers;

    for (auto& [entity, pos] : entity_positions) {
        if (!should_transfer(entity, pos)) continue;

        ZoneID new_zone = get_zone_for_position(pos);
        if (new_zone == INVALID_ZONE) continue;

        auto it = entity_zones_.find(entity);
        ZoneID old_zone = (it != entity_zones_.end()) ? it->second : INVALID_ZONE;
        if (old_zone == new_zone) continue;

        TransferRequest req;
        req.entity    = entity;
        req.from_zone = old_zone;
        req.to_zone   = new_zone;
        req.state.id       = entity;
        req.state.position = pos;
        transfers.push_back(req);

        entity_zones_[entity] = new_zone;
    }

    return transfers;
}

const ZoneConfig* ZoneManager::get_zone(ZoneID id) const {
    auto it = zones_.find(id);
    return it != zones_.end() ? &it->second : nullptr;
}

std::vector<ZoneID> ZoneManager::get_adjacent_zones(ZoneID id) const {
    std::vector<ZoneID> adjacent;
    auto it = zones_.find(id);
    if (it == zones_.end()) return adjacent;

    const auto& zone = it->second;
    for (auto& [other_id, other] : zones_) {
        if (other_id == id) continue;
        bool overlaps =
            zone.bounds_min.x <= other.bounds_max.x + boundary_margin_ &&
            zone.bounds_max.x >= other.bounds_min.x - boundary_margin_ &&
            zone.bounds_min.y <= other.bounds_max.y + boundary_margin_ &&
            zone.bounds_max.y >= other.bounds_min.y - boundary_margin_ &&
            zone.bounds_min.z <= other.bounds_max.z + boundary_margin_ &&
            zone.bounds_max.z >= other.bounds_min.z - boundary_margin_;
        if (overlaps) adjacent.push_back(other_id);
    }
    return adjacent;
}

} // namespace lumios::net
