#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace d1assist::network
{
class TcpConnection
{
public:
    TcpConnection() noexcept;
    ~TcpConnection();

    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    static TcpConnection connectTo(const std::string& host, std::uint16_t port);

    bool isOpen() const noexcept;
    void sendAll(std::span<const std::uint8_t> data);
    bool receiveAll(std::span<std::uint8_t> data);
    void shutdown() noexcept;
    void close() noexcept;

private:
    struct Impl;

    explicit TcpConnection(std::intptr_t nativeSocket);
    std::unique_ptr<Impl> impl_;

    friend class TcpListener;
};

class TcpListener
{
public:
    TcpListener() noexcept;
    ~TcpListener();

    TcpListener(TcpListener&& other) noexcept;
    TcpListener& operator=(TcpListener&& other) noexcept;

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    void listen(const std::string& host, std::uint16_t port);
    TcpConnection accept();
    void close() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
