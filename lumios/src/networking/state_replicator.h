#pragma once

#include "net_types.h"
#include "net_transport.h"
#include <unordered_map>

namespace lumios::net {

class StateReplicator {
public:
    void set_transport(NetworkTransport* transport) { transport_ = transport; }

    void track_entity(EntityNetID id, const EntityState& state);
    void untrack_entity(EntityNetID id);

    void update_state(EntityNetID id, const EntityState& state);

    void send_full_snapshot(ClientID client);
    void send_delta(ClientID client);
    void broadcast_deltas();

    void on_receive_snapshot(const NetworkMessage& msg,
                             std::unordered_map<EntityNetID, EntityState>& out_states);

    void set_snapshot_rate(float hz) { snapshot_interval_ = 1.0f / hz; }

private:
    NetworkTransport* transport_ = nullptr;

    struct TrackedEntity {
        EntityState current;
        EntityState last_sent;
        bool dirty = true;
    };

    std::unordered_map<EntityNetID, TrackedEntity> entities_;
    float snapshot_interval_ = 1.0f / 20.0f;
    float snapshot_timer_    = 0.0f;

    bool has_changed(const EntityState& a, const EntityState& b) const;
    NetworkMessage build_snapshot_msg(const std::vector<EntityState>& states) const;
    NetworkMessage build_delta_msg(const std::vector<EntityState>& changed) const;
};

} // namespace lumios::net
