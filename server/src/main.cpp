#include "device_registry.h"
#include "broker.h"

#include "d1assist/network/tcp_connection.h"
#include "d1assist/protocol/packet.h"

#include <atomic>
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <system_error>

namespace
{
std::mutex outputMutex;

void logLine(const std::string& message)
{
    std::lock_guard lock(outputMutex);
    std::cout << message << std::endl;
}

std::uint16_t parsePort(const char* value)
{
    unsigned int port = 0;
    const std::string text(value);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        port == 0 || port > 65'535)
    {
        throw std::invalid_argument("Port must be a number between 1 and 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

void handleClient(
    std::shared_ptr<d1assist::network::TcpConnection> connection,
    const std::shared_ptr<DeviceRegistry>& registry,
    const std::shared_ptr<Broker>& broker,
    std::uint64_t connectionId
)
{
    std::uint64_t registeredPublicId = 0;
    bool markedOnline = false;
    std::shared_ptr<Broker::Peer> peer;

    try
    {
        d1assist::protocol::Packet packet{};
        if (!d1assist::protocol::receivePacket(*connection, packet))
        {
            return;
        }
        if (packet.type != d1assist::protocol::MessageType::RegisterDevice)
        {
            throw std::runtime_error("The first client message must register the device.");
        }

        d1assist::protocol::PacketReader requestReader(packet.payload);
        const auto deviceUuid = requestReader.readString(36);
        if (!requestReader.empty())
        {
            throw std::runtime_error("The registration request contains unexpected data.");
        }

        registeredPublicId = registry->getOrAssignPublicId(deviceUuid);
        markedOnline = registry->markOnline(registeredPublicId, connectionId);

        d1assist::protocol::PacketWriter response;
        response.writeUInt16(markedOnline ? 0 : 1);
        response.writeUInt64(registeredPublicId);
        d1assist::protocol::sendPacket(
            *connection,
            d1assist::protocol::MessageType::RegisterDeviceResponse,
            response.data()
        );

        if (!markedOnline)
        {
            logLine("Registration rejected: this device is already online.");
            return;
        }

        peer = std::make_shared<Broker::Peer>();
        peer->publicId = registeredPublicId;
        peer->connectionId = connectionId;
        peer->connection = std::move(connection);
        broker->addPeer(peer);

        logLine("Device registered and online. Public ID: " + std::to_string(registeredPublicId));

        while (d1assist::protocol::receivePacket(*peer->connection, packet))
        {
            broker->handlePacket(peer, packet);
        }
    }
    catch (const std::exception& error)
    {
        logLine(std::string("Client connection error: ") + error.what());
    }

    if (peer)
    {
        broker->removePeer(peer);
    }

    if (markedOnline)
    {
        registry->markOffline(registeredPublicId, connectionId);
        logLine("Device offline. Public ID: " + std::to_string(registeredPublicId));
    }
}

void printUsage()
{
    std::cout << "Usage: D1AssistServer.exe [port] [registry-file]\n"
                 "Defaults: port 5577; registry file ./server-data/devices.db\n"
                 "The server listens on 127.0.0.1 by default in this prototype.\n";
}
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc > 3)
        {
            printUsage();
            return 2;
        }

        const std::uint16_t port = argc >= 2 ? parsePort(argv[1]) : 5577;
        const auto registryPath = argc >= 3
            ? std::filesystem::path(argv[2])
            : std::filesystem::path("server-data") / "devices.db";

        auto registry = std::make_shared<DeviceRegistry>(registryPath);
        auto broker = std::make_shared<Broker>();
        d1assist::network::TcpListener listener;
        listener.listen("127.0.0.1", port);

        logLine("D1AssistServer listening on 127.0.0.1:" + std::to_string(port));
        logLine("Device registry: " + std::filesystem::absolute(registryPath).string());
        logLine("Press Ctrl+C to stop the server.");

        std::atomic<std::uint64_t> nextConnectionId{1};
        while (true)
        {
            auto connection = listener.accept();
            const auto connectionId = nextConnectionId.fetch_add(1);
            std::thread(
                handleClient,
                std::make_shared<d1assist::network::TcpConnection>(std::move(connection)),
                registry,
                broker,
                connectionId
            ).detach();
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "D1AssistServer error: " << error.what() << '\n';
        return 1;
    }
}
