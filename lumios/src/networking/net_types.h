#pragma once

#include "../core/types.h"
#include "../math/math.h"
#include <string>
#include <vector>
#include <functional>

namespace lumios::net {

using ClientID = u32;
using ZoneID   = u32;
using EntityNetID = u64;

constexpr ClientID INVALID_CLIENT = UINT32_MAX;
constexpr ZoneID   INVALID_ZONE   = UINT32_MAX;

enum class MessageType : u16 {
    Connect        = 1,
    Disconnect     = 2,
    Input          = 10,
    StateSnapshot  = 20,
    StateDelta     = 21,
    EntitySpawn    = 30,
    EntityDestroy  = 31,
    ZoneTransfer   = 40,
    Ping           = 100,
    Pong           = 101,
    Custom         = 200,
};

struct NetworkMessage {
    MessageType type;
    ClientID    sender = INVALID_CLIENT;
    std::vector<u8> payload;

    template<typename T>
    void write(const T& value) {
        const u8* ptr = reinterpret_cast<const u8*>(&value);
        payload.insert(payload.end(), ptr, ptr + sizeof(T));
    }

    template<typename T>
    T read(size_t offset) const {
        T value{};
        if (offset + sizeof(T) <= payload.size())
            memcpy(&value, payload.data() + offset, sizeof(T));
        return value;
    }
};

struct EntityState {
    EntityNetID id;
    glm::vec3   position;
    glm::vec3   rotation;
    glm::vec3   velocity;
    u32         component_mask;
};

struct ClientInput {
    float     move_x, move_y;
    float     look_yaw, look_pitch;
    u32       button_mask;
    u32       sequence;
};

} // namespace lumios::net
