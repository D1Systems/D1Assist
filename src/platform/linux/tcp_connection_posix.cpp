#include "d1assist/network/tcp_connection.h"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace d1assist::network
{
namespace
{
std::string socketError(const std::string& operation)
{
    return operation + " failed: " + std::strerror(errno);
}

void closeSocket(int socket) noexcept
{
    if (socket >= 0)
    {
        ::close(socket);
    }
}
}

struct TcpConnection::Impl
{
    explicit Impl(int value) : socket(value) {}
    int socket = -1;
};

struct TcpListener::Impl
{
    explicit Impl(int value) : socket(value) {}
    int socket = -1;
};

TcpConnection::TcpConnection() noexcept = default;

TcpConnection::TcpConnection(std::intptr_t nativeSocket)
    : impl_(std::make_unique<Impl>(static_cast<int>(nativeSocket)))
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
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const auto service = std::to_string(port);
    const int lookupResult = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
    if (lookupResult != 0)
    {
        throw std::runtime_error("Could not resolve server address: " +
                                 std::string(gai_strerror(lookupResult)));
    }

    int connectedSocket = -1;
    for (auto* address = addresses; address != nullptr; address = address->ai_next)
    {
        const int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate < 0)
        {
            continue;
        }
        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0)
        {
            connectedSocket = candidate;
            break;
        }
        closeSocket(candidate);
    }

    freeaddrinfo(addresses);
    if (connectedSocket < 0)
    {
        throw std::runtime_error(socketError("Connecting to server"));
    }

    return TcpConnection(connectedSocket);
}

bool TcpConnection::isOpen() const noexcept
{
    return impl_ != nullptr && impl_->socket >= 0;
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
        const auto result = send(impl_->socket, data.data() + sent, data.size() - sent, 0);
        if (result < 0)
        {
            throw std::runtime_error(socketError("Sending data"));
        }
        if (result == 0)
        {
            throw std::runtime_error("The connection closed while sending data.");
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
        const auto result = recv(impl_->socket, data.data() + received, data.size() - received, 0);
        if (result == 0)
        {
            if (received == 0)
            {
                return false;
            }
            throw std::runtime_error("The peer closed the connection during a message.");
        }
        if (result < 0)
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
        ::shutdown(impl_->socket, SHUT_RDWR);
    }
}

void TcpConnection::close() noexcept
{
    if (impl_ != nullptr)
    {
        closeSocket(impl_->socket);
        impl_->socket = -1;
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
        throw std::runtime_error("Could not resolve listen address: " +
                                 std::string(gai_strerror(lookupResult)));
    }

    int listeningSocket = -1;
    for (auto* address = addresses; address != nullptr; address = address->ai_next)
    {
        const int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate < 0)
        {
            continue;
        }

        const int reuseAddress = 1;
        setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
        if (bind(candidate, address->ai_addr, address->ai_addrlen) == 0 &&
            ::listen(candidate, SOMAXCONN) == 0)
        {
            listeningSocket = candidate;
            break;
        }
        closeSocket(candidate);
    }

    freeaddrinfo(addresses);
    if (listeningSocket < 0)
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
    const int clientSocket = ::accept(impl_->socket, nullptr, nullptr);
    if (clientSocket < 0)
    {
        throw std::runtime_error(socketError("Accepting client connection"));
    }
    return TcpConnection(clientSocket);
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
