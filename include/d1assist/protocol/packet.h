#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "d1assist/network/tcp_connection.h"
#include "d1assist/protocol/message_type.h"

namespace d1assist::protocol
{
struct Packet
{
    MessageType type;
    std::vector<std::uint8_t> payload;
};

class PacketWriter
{
public:
    void writeUInt16(std::uint16_t value);
    void writeUInt32(std::uint32_t value);
    void writeUInt64(std::uint64_t value);
    void writeString(const std::string& value);
    void writeBytes(std::span<const std::uint8_t> value);

    const std::vector<std::uint8_t>& data() const noexcept;

private:
    std::vector<std::uint8_t> data_;
};

class PacketReader
{
public:
    explicit PacketReader(std::span<const std::uint8_t> data) noexcept;

    std::uint16_t readUInt16();
    std::uint32_t readUInt32();
    std::uint64_t readUInt64();
    std::string readString(std::size_t maximumLength = 128);
    std::vector<std::uint8_t> readBytes(std::size_t maximumLength = 1024U * 1024U);
    bool empty() const noexcept;

private:
    void require(std::size_t count) const;

    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};

void sendPacket(
    network::TcpConnection& connection,
    MessageType type,
    std::span<const std::uint8_t> payload
);

bool receivePacket(network::TcpConnection& connection, Packet& packet);
}
