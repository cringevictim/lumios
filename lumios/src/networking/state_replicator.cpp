#include "state_replicator.h"

namespace lumios::net {

void StateReplicator::track_entity(EntityNetID id, const EntityState& state) {
    entities_[id] = {state, state, true};
}

void StateReplicator::untrack_entity(EntityNetID id) {
    entities_.erase(id);
}

void StateReplicator::update_state(EntityNetID id, const EntityState& state) {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        it->second.current = state;
        it->second.dirty = has_changed(it->second.current, it->second.last_sent);
    }
}

bool StateReplicator::has_changed(const EntityState& a, const EntityState& b) const {
    const float pos_threshold = 0.001f;
    const float rot_threshold = 0.01f;

    if (glm::length(a.position - b.position) > pos_threshold) return true;
    if (glm::length(a.rotation - b.rotation) > rot_threshold) return true;
    if (glm::length(a.velocity - b.velocity) > pos_threshold) return true;
    return false;
}

void StateReplicator::send_full_snapshot(ClientID client) {
    if (!transport_) return;

    std::vector<EntityState> states;
    states.reserve(entities_.size());
    for (auto& [id, tracked] : entities_) {
        states.push_back(tracked.current);
    }

    NetworkMessage msg = build_snapshot_msg(states);
    transport_->send_reliable(client, msg);
}

void StateReplicator::send_delta(ClientID client) {
    if (!transport_) return;

    std::vector<EntityState> changed;
    for (auto& [id, tracked] : entities_) {
        if (tracked.dirty) {
            changed.push_back(tracked.current);
            tracked.last_sent = tracked.current;
            tracked.dirty = false;
        }
    }

    if (!changed.empty()) {
        NetworkMessage msg = build_delta_msg(changed);
        transport_->send_unreliable(client, msg);
    }
}

void StateReplicator::broadcast_deltas() {
    if (!transport_) return;

    std::vector<EntityState> changed;
    for (auto& [id, tracked] : entities_) {
        if (tracked.dirty) {
            changed.push_back(tracked.current);
            tracked.last_sent = tracked.current;
            tracked.dirty = false;
        }
    }

    if (!changed.empty()) {
        NetworkMessage msg = build_delta_msg(changed);
        transport_->broadcast_unreliable(msg);
    }
}

void StateReplicator::on_receive_snapshot(const NetworkMessage& msg,
    std::unordered_map<EntityNetID, EntityState>& out_states) {
    size_t offset = 0;
    u32 count = msg.read<u32>(offset);
    offset += sizeof(u32);

    for (u32 i = 0; i < count && offset + sizeof(EntityState) <= msg.payload.size(); i++) {
        EntityState state = msg.read<EntityState>(offset);
        offset += sizeof(EntityState);
        out_states[state.id] = state;
    }
}

NetworkMessage StateReplicator::build_snapshot_msg(const std::vector<EntityState>& states) const {
    NetworkMessage msg;
    msg.type = MessageType::StateSnapshot;
    msg.write(static_cast<u32>(states.size()));
    for (const auto& s : states) msg.write(s);
    return msg;
}

NetworkMessage StateReplicator::build_delta_msg(const std::vector<EntityState>& changed) const {
    NetworkMessage msg;
    msg.type = MessageType::StateDelta;
    msg.write(static_cast<u32>(changed.size()));
    for (const auto& s : changed) msg.write(s);
    return msg;
}

} // namespace lumios::net
