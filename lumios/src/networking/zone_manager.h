#pragma once

#include "net_types.h"
#include <unordered_map>
#include <unordered_set>

namespace lumios::net {

struct ZoneConfig {
    ZoneID    id;
    glm::vec3 bounds_min;
    glm::vec3 bounds_max;
    std::string server_address;
    u16 server_port = 0;
};

class ZoneManager {
public:
    void add_zone(const ZoneConfig& config);
    void remove_zone(ZoneID id);

    ZoneID get_zone_for_position(const glm::vec3& position) const;

    bool should_transfer(EntityNetID entity, const glm::vec3& new_position) const;
    void register_entity(EntityNetID entity, ZoneID zone);
    void unregister_entity(EntityNetID entity);

    struct TransferRequest {
        EntityNetID entity;
        ZoneID      from_zone;
        ZoneID      to_zone;
        EntityState state;
    };

    std::vector<TransferRequest> process_transfers(
        const std::unordered_map<EntityNetID, glm::vec3>& entity_positions);

    const ZoneConfig* get_zone(ZoneID id) const;
    std::vector<ZoneID> get_adjacent_zones(ZoneID id) const;

private:
    std::unordered_map<ZoneID, ZoneConfig>    zones_;
    std::unordered_map<EntityNetID, ZoneID>   entity_zones_;
    float boundary_margin_ = 5.0f;
};

} // namespace lumios::net
