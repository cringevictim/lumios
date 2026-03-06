#pragma once

#include "net_types.h"

namespace lumios::net {

class NetworkTransport {
public:
    virtual ~NetworkTransport() = default;

    virtual bool start_server(u16 port) = 0;
    virtual bool connect(const std::string& host, u16 port) = 0;
    virtual void disconnect() = 0;

    virtual void send_reliable(ClientID target, const NetworkMessage& msg) = 0;
    virtual void send_unreliable(ClientID target, const NetworkMessage& msg) = 0;
    virtual void broadcast_reliable(const NetworkMessage& msg) = 0;
    virtual void broadcast_unreliable(const NetworkMessage& msg) = 0;

    virtual void poll() = 0;

    using OnConnect    = std::function<void(ClientID)>;
    using OnDisconnect = std::function<void(ClientID)>;
    using OnMessage    = std::function<void(ClientID, const NetworkMessage&)>;

    void set_on_connect(OnConnect cb)       { on_connect_    = std::move(cb); }
    void set_on_disconnect(OnDisconnect cb) { on_disconnect_ = std::move(cb); }
    void set_on_message(OnMessage cb)       { on_message_    = std::move(cb); }

    virtual bool is_server() const = 0;
    virtual bool is_connected() const = 0;
    virtual u32  client_count() const = 0;

protected:
    OnConnect    on_connect_;
    OnDisconnect on_disconnect_;
    OnMessage    on_message_;
};

} // namespace lumios::net
