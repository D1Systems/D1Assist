#include "d1assist/network/tcp_connection.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

namespace d1assist::network
{
namespace
{
class WinsockRuntime
{
public:
    WinsockRuntime()
    {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0)
        {
            throw std::runtime_error("WSAStartup failed with code " + std::to_string(result));
        }
    }

    ~WinsockRuntime()
    {
        WSACleanup();
    }
};

void ensureWinsock()
{
    static WinsockRuntime runtime;
    static_cast<void>(runtime);
}

std::string socketError(const std::string& operation)
{
    return operation + " failed with Winsock code " + std::to_string(WSAGetLastError());
}

void closeSocket(SOCKET socket) noexcept
{
    if (socket != INVALID_SOCKET)
    {
        closesocket(socket);
    }
}
}

struct TcpConnection::Impl
{
    explicit Impl(SOCKET value) : socket(value) {}
    SOCKET socket = INVALID_SOCKET;
};

struct TcpListener::Impl
{
    explicit Impl(SOCKET value) : socket(value) {}
    SOCKET socket = INVALID_SOCKET;
};

TcpConnection::TcpConnection() noexcept = default;

TcpConnection::TcpConnection(std::intptr_t nativeSocket)
    : impl_(std::make_unique<Impl>(static_cast<SOCKET>(nativeSocket)))
{
}

TcpConnection::~TcpConnection()
{
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept = default;

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept = default;

TcpConnection TcpConnection::connectTo(const std::string& host, std::uint16_t port)
{
    ensureWinsock();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const auto service = std::to_string(port);
    const int lookupResult = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
    if (lookupResult != 0)
    {
        throw std::runtime_error("Could not resolve server address (Winsock code " +
                                 std::to_string(lookupResult) + ").");
    }

    SOCKET connectedSocket = INVALID_SOCKET;
    for (auto* address = addresses; address != nullptr; address = address->ai_next)
    {
        const SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET)
        {
            continue;
        }
        if (connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
        {
            connectedSocket = candidate;
            break;
        }
        closeSocket(candidate);
    }

    freeaddrinfo(addresses);
    if (connectedSocket == INVALID_SOCKET)
    {
        throw std::runtime_error(socketError("Connecting to server"));
    }

    return TcpConnection(static_cast<std::intptr_t>(connectedSocket));
}

bool TcpConnection::isOpen() const noexcept
{
    return impl_ != nullptr && impl_->socket != INVALID_SOCKET;
}

void TcpConnection::sendAll(std::span<const std::uint8_t> data)
{
    if (!isOpen())
    {
        throw std::runtime_error("Cannot send data on a closed connection.");
    }

    std::size_t sent = 0;
    while (sent < data.size())
    {
        const auto remaining = data.size() - sent;
        const int chunkSize = static_cast<int>(
            remaining > static_cast<std::size_t>(std::numeric_limits<int>::max())
                ? std::numeric_limits<int>::max()
                : remaining
        );
        const int result = send(
            impl_->socket,
            reinterpret_cast<const char*>(data.data() + sent),
            chunkSize,
            0
        );
        if (result == SOCKET_ERROR || result == 0)
        {
            throw std::runtime_error(socketError("Sending data"));
        }
        sent += static_cast<std::size_t>(result);
    }
}

bool TcpConnection::receiveAll(std::span<std::uint8_t> data)
{
    if (!isOpen())
    {
        throw std::runtime_error("Cannot receive data on a closed connection.");
    }

    std::size_t received = 0;
    while (received < data.size())
    {
        const auto remaining = data.size() - received;
        const int chunkSize = static_cast<int>(
            remaining > static_cast<std::size_t>(std::numeric_limits<int>::max())
                ? std::numeric_limits<int>::max()
                : remaining
        );
        const int result = recv(
            impl_->socket,
            reinterpret_cast<char*>(data.data() + received),
            chunkSize,
            0
        );
        if (result == 0)
        {
            if (received == 0)
            {
                return false;
            }
            throw std::runtime_error("The peer closed the connection during a message.");
        }
        if (result == SOCKET_ERROR)
        {
            throw std::runtime_error(socketError("Receiving data"));
        }
        received += static_cast<std::size_t>(result);
    }
    return true;
}

void TcpConnection::shutdown() noexcept
{
    if (isOpen())
    {
        ::shutdown(impl_->socket, SD_BOTH);
    }
}

void TcpConnection::close() noexcept
{
    if (impl_ != nullptr)
    {
        closeSocket(impl_->socket);
        impl_->socket = INVALID_SOCKET;
        impl_.reset();
    }
}

TcpListener::TcpListener() noexcept = default;

TcpListener::~TcpListener()
{
    close();
}

TcpListener::TcpListener(TcpListener&& other) noexcept = default;

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept = default;

void TcpListener::listen(const std::string& host, std::uint16_t port)
{
    ensureWinsock();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = host.empty() ? AI_PASSIVE : 0;

    addrinfo* addresses = nullptr;
    const auto service = std::to_string(port);
    const char* hostName = host.empty() ? nullptr : host.c_str();
    const int lookupResult = getaddrinfo(hostName, service.c_str(), &hints, &addresses);
    if (lookupResult != 0)
    {
        throw std::runtime_error("Could not resolve listen address (Winsock code " +
                                 std::to_string(lookupResult) + ").");
    }

    SOCKET listeningSocket = INVALID_SOCKET;
    for (auto* address = addresses; address != nullptr; address = address->ai_next)
    {
        const SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET)
        {
            continue;
        }

        BOOL reuseAddress = TRUE;
        setsockopt(
            candidate,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuseAddress),
            sizeof(reuseAddress)
        );
        if (bind(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0 &&
            ::listen(candidate, SOMAXCONN) == 0)
        {
            listeningSocket = candidate;
            break;
        }
        closeSocket(candidate);
    }

    freeaddrinfo(addresses);
    if (listeningSocket == INVALID_SOCKET)
    {
        throw std::runtime_error(socketError("Starting TCP listener"));
    }

    close();
    impl_ = std::make_unique<Impl>(listeningSocket);
}

TcpConnection TcpListener::accept()
{
    if (impl_ == nullptr)
    {
        throw std::runtime_error("The TCP listener is not active.");
    }
    const SOCKET clientSocket = ::accept(impl_->socket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET)
    {
        throw std::runtime_error(socketError("Accepting client connection"));
    }
    return TcpConnection(static_cast<std::intptr_t>(clientSocket));
}

void TcpListener::close() noexcept
{
    if (impl_ != nullptr)
    {
        closeSocket(impl_->socket);
        impl_.reset();
    }
}
}
