#include "broker.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

using d1assist::protocol::MessageType;
using d1assist::protocol::Packet;
using d1assist::protocol::PacketReader;
using d1assist::protocol::PacketWriter;

namespace
{
std::mutex outputMutex;

void requireEmpty(const PacketReader& reader)
{
    if (!reader.empty())
    {
        throw std::runtime_error("The broker message contains unexpected data.");
    }
}

void readRequestId(PacketReader& reader, std::uint64_t& requestId)
{
    requestId = reader.readUInt64();
}
}

void Broker::send(
    const std::shared_ptr<Peer>& peer,
    MessageType type,
    const PacketWriter& packet
)
{
    send(peer, type, packet.data());
}

void Broker::send(
    const std::shared_ptr<Peer>& peer,
    MessageType type,
    std::span<const std::uint8_t> payload
)
{
    std::lock_guard lock(peer->sendMutex);
    d1assist::protocol::sendPacket(*peer->connection, type, payload);
}

void Broker::sendStatus(
    const std::shared_ptr<Peer>& peer,
    MessageType type,
    std::uint64_t requestId,
    std::uint16_t status
)
{
    PacketWriter response;
    response.writeUInt64(requestId);
    response.writeUInt16(status);
    send(peer, type, response);
}

void Broker::addPeer(const std::shared_ptr<Peer>& peer)
{
    std::lock_guard lock(mutex_);
    if (!peersByPublicId_.emplace(peer->publicId, peer).second)
    {
        throw std::runtime_error("The public ID is already registered in the broker.");
    }
}

bool Broker::isBusyLocked(const std::shared_ptr<Peer>& peer) const
{
    for (const auto& [id, request] : requests_)
    {
        static_cast<void>(id);
        if (request.controller == peer || request.controlled == peer)
        {
            return true;
        }
    }
    return false;
}

void Broker::requestConnection(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    const auto targetId = reader.readUInt64();
    requireEmpty(reader);

    std::shared_ptr<Peer> controlled;
    std::uint64_t requestId = 0;
    std::string rejection;
    {
        std::lock_guard lock(mutex_);
        const auto target = peersByPublicId_.find(targetId);
        if (target == peersByPublicId_.end())
        {
            rejection = "O dispositivo de destino não está online.";
        }
        else if (target->second == peer)
        {
            rejection = "Não é possível conectar a este próprio dispositivo.";
        }
        else if (isBusyLocked(peer) || isBusyLocked(target->second))
        {
            rejection = "Um dos dispositivos já está em outra solicitação ou sessão.";
        }
        else
        {
            controlled = target->second;
            requestId = nextRequestId_++;
            requests_.emplace(requestId, Request{requestId, peer, controlled, RequestStage::Consent});
        }
    }

    if (!controlled)
    {
        PacketWriter response;
        response.writeString(rejection);
        send(peer, MessageType::ConnectRejected, response);
        return;
    }

    PacketWriter incoming;
    incoming.writeUInt64(requestId);
    incoming.writeUInt64(peer->publicId);
    send(controlled, MessageType::IncomingConnection, incoming);
}

void Broker::respondToConnection(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    std::uint64_t requestId = 0;
    readRequestId(reader, requestId);
    const auto accepted = reader.readUInt16();
    requireEmpty(reader);
    if (accepted > 1)
    {
        throw std::runtime_error("The consent response has an invalid value.");
    }

    std::shared_ptr<Peer> controller;
    {
        std::lock_guard lock(mutex_);
        const auto request = requests_.find(requestId);
        if (request == requests_.end() || request->second.controlled != peer ||
            request->second.stage != RequestStage::Consent)
        {
            throw std::runtime_error("The consent response does not match a pending request.");
        }
        controller = request->second.controller;
        if (accepted == 1)
        {
            request->second.stage = RequestStage::Authentication;
        }
        else
        {
            requests_.erase(request);
        }
    }

    sendStatus(
        controller,
        accepted == 1 ? MessageType::ConnectionResponse : MessageType::ConnectRejected,
        requestId,
        accepted
    );
}

void Broker::forwardAuthentication(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    std::uint64_t requestId = 0;
    readRequestId(reader, requestId);
    const auto password = reader.readString(6);
    requireEmpty(reader);
    if (password.size() != 6 ||
        !std::all_of(password.begin(), password.end(), [](unsigned char digit) {
            return digit >= '0' && digit <= '9';
        }))
    {
        throw std::runtime_error("The temporary password must contain six digits.");
    }

    std::shared_ptr<Peer> controlled;
    {
        std::lock_guard lock(mutex_);
        const auto request = requests_.find(requestId);
        if (request == requests_.end() || request->second.controller != peer ||
            request->second.stage != RequestStage::Authentication)
        {
            throw std::runtime_error("The authentication request does not match a consented connection.");
        }
        controlled = request->second.controlled;
    }

    send(controlled, MessageType::AuthRequest, packet.payload);
}

void Broker::respondToAuthentication(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    std::uint64_t requestId = 0;
    readRequestId(reader, requestId);
    const auto accepted = reader.readUInt16();
    requireEmpty(reader);
    if (accepted > 1)
    {
        throw std::runtime_error("The authentication response has an invalid value.");
    }

    std::shared_ptr<Peer> controller;
    std::shared_ptr<Peer> controlled;
    {
        std::lock_guard lock(mutex_);
        const auto request = requests_.find(requestId);
        if (request == requests_.end() || request->second.controlled != peer ||
            request->second.stage != RequestStage::Authentication)
        {
            throw std::runtime_error("The authentication response does not match a pending request.");
        }
        controller = request->second.controller;
        controlled = request->second.controlled;
        if (accepted == 1)
        {
            request->second.stage = RequestStage::Active;
        }
        else
        {
            requests_.erase(request);
        }
    }

    if (accepted == 0)
    {
        sendStatus(controller, MessageType::AuthDenied, requestId, 0);
        return;
    }

    sendStatus(controller, MessageType::AuthAccepted, requestId, 1);
    PacketWriter controllerSession;
    controllerSession.writeUInt64(requestId);
    controllerSession.writeUInt16(0);
    send(controller, MessageType::SessionStart, controllerSession);
    PacketWriter controlledSession;
    controlledSession.writeUInt64(requestId);
    controlledSession.writeUInt16(1);
    send(controlled, MessageType::SessionStart, controlledSession);
}

void Broker::endSession(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    std::uint64_t requestId = 0;
    readRequestId(reader, requestId);
    requireEmpty(reader);

    std::shared_ptr<Peer> other;
    {
        std::lock_guard lock(mutex_);
        const auto request = requests_.find(requestId);
        if (request == requests_.end() ||
            (request->second.controller != peer && request->second.controlled != peer))
        {
            return;
        }
        other = request->second.controller == peer
            ? request->second.controlled
            : request->second.controller;
        requests_.erase(request);
    }

    PacketWriter response;
    response.writeUInt64(requestId);
    send(other, MessageType::SessionEnd, response);
}

void Broker::relaySessionPacket(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    PacketReader reader(packet.payload);
    std::uint64_t requestId = 0;
    readRequestId(reader, requestId);

    std::shared_ptr<Peer> other;
    {
        std::lock_guard lock(mutex_);
        const auto request = requests_.find(requestId);
        if (request == requests_.end() || request->second.stage != RequestStage::Active)
        {
            throw std::runtime_error("Session data was sent outside an active session.");
        }

        const auto fromController = request->second.controller == peer;
        const auto fromControlled = request->second.controlled == peer;
        if (!fromController && !fromControlled)
        {
            throw std::runtime_error("The device is not a participant in this session.");
        }
        if ((packet.type == MessageType::Frame && !fromControlled) ||
            (packet.type != MessageType::Frame && !fromController))
        {
            throw std::runtime_error("The session packet is not allowed from this participant.");
        }
        other = fromController ? request->second.controlled : request->second.controller;
    }

    send(other, packet.type, packet.payload);
}

void Broker::handlePacket(const std::shared_ptr<Peer>& peer, const Packet& packet)
{
    switch (packet.type)
    {
    case MessageType::ConnectRequest:
        requestConnection(peer, packet);
        return;
    case MessageType::ConnectionResponse:
        respondToConnection(peer, packet);
        return;
    case MessageType::AuthRequest:
        forwardAuthentication(peer, packet);
        return;
    case MessageType::AuthResponse:
        respondToAuthentication(peer, packet);
        return;
    case MessageType::SessionEnd:
        endSession(peer, packet);
        return;
    case MessageType::Frame:
    case MessageType::MouseMove:
    case MessageType::MouseButton:
    case MessageType::MouseWheel:
    case MessageType::KeyDown:
    case MessageType::KeyUp:
        relaySessionPacket(peer, packet);
        return;
    default:
        throw std::runtime_error("The client sent a message not supported by the broker.");
    }
}

void Broker::removePeer(const std::shared_ptr<Peer>& peer) noexcept
{
    std::vector<std::pair<std::shared_ptr<Peer>, std::uint64_t>> notify;
    {
        std::lock_guard lock(mutex_);
        const auto online = peersByPublicId_.find(peer->publicId);
        if (online != peersByPublicId_.end() && online->second == peer)
        {
            peersByPublicId_.erase(online);
        }

        for (auto request = requests_.begin(); request != requests_.end();)
        {
            if (request->second.controller == peer || request->second.controlled == peer)
            {
                const auto other = request->second.controller == peer
                    ? request->second.controlled
                    : request->second.controller;
                notify.emplace_back(other, request->first);
                request = requests_.erase(request);
            }
            else
            {
                ++request;
            }
        }
    }

    for (const auto& [other, requestId] : notify)
    {
        try
        {
            PacketWriter response;
            response.writeUInt64(requestId);
            send(other, MessageType::SessionEnd, response);
        }
        catch (const std::exception& error)
        {
            std::lock_guard outputLock(outputMutex);
            std::cerr << "Could not notify the other session participant: "
                      << error.what() << '\n';
        }
    }
}
