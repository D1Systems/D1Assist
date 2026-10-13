#pragma once

#include "d1assist/network/tcp_connection.h"
#include "d1assist/protocol/packet.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

class Broker
{
public:
    struct Peer
    {
        std::uint64_t publicId = 0;
        std::uint64_t connectionId = 0;
        std::shared_ptr<d1assist::network::TcpConnection> connection;
        std::mutex sendMutex;
    };

    void addPeer(const std::shared_ptr<Peer>& peer);
    void handlePacket(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void removePeer(const std::shared_ptr<Peer>& peer) noexcept;

private:
    enum class RequestStage
    {
        Consent,
        Authentication,
        Active
    };

    struct Request
    {
        std::uint64_t id = 0;
        std::shared_ptr<Peer> controller;
        std::shared_ptr<Peer> controlled;
        RequestStage stage = RequestStage::Consent;
    };

    static void send(
        const std::shared_ptr<Peer>& peer,
        d1assist::protocol::MessageType type,
        const d1assist::protocol::PacketWriter& packet
    );
    static void send(
        const std::shared_ptr<Peer>& peer,
        d1assist::protocol::MessageType type,
        std::span<const std::uint8_t> payload
    );
    static void sendStatus(
        const std::shared_ptr<Peer>& peer,
        d1assist::protocol::MessageType type,
        std::uint64_t requestId,
        std::uint16_t status
    );
    bool isBusyLocked(const std::shared_ptr<Peer>& peer) const;
    void requestConnection(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void respondToConnection(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void forwardAuthentication(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void respondToAuthentication(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void endSession(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);
    void relaySessionPacket(const std::shared_ptr<Peer>& peer, const d1assist::protocol::Packet& packet);

    mutable std::mutex mutex_;
    std::map<std::uint64_t, std::shared_ptr<Peer>> peersByPublicId_;
    std::map<std::uint64_t, Request> requests_;
    std::uint64_t nextRequestId_ = 1;
};
