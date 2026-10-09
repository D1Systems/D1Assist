#include "d1assist/protocol/packet.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace d1assist::protocol
{
namespace
{
constexpr std::uint32_t PACKET_MAGIC = 0x44314153U;
constexpr std::uint16_t PROTOCOL_VERSION = 1;
constexpr std::size_t HEADER_SIZE = 12;
constexpr std::uint32_t MAX_PAYLOAD_SIZE = 1024U * 1024U;

bool isKnownMessageType(std::uint16_t value)
{
    switch (static_cast<MessageType>(value))
    {
    case MessageType::Hello:
    case MessageType::HelloAck:
    case MessageType::RegisterDevice:
    case MessageType::RegisterDeviceResponse:
    case MessageType::ConnectRequest:
    case MessageType::IncomingConnection:
    case MessageType::ConnectRejected:
    case MessageType::AuthRequest:
    case MessageType::AuthResponse:
    case MessageType::AuthAccepted:
    case MessageType::AuthDenied:
    case MessageType::SessionStart:
    case MessageType::SessionEnd:
    case MessageType::Frame:
    case MessageType::MouseMove:
    case MessageType::MouseButton:
    case MessageType::MouseWheel:
    case MessageType::KeyDown:
    case MessageType::KeyUp:
    case MessageType::Clipboard:
    case MessageType::FileTransferStart:
    case MessageType::FileTransferData:
    case MessageType::FileTransferEnd:
        return true;
    }
    return false;
}

void writeBigEndian(
    std::span<std::uint8_t> output,
    std::size_t offset,
    std::uint64_t value,
    std::size_t byteCount
)
{
    for (std::size_t index = 0; index < byteCount; ++index)
    {
        const auto shift = (byteCount - index - 1U) * 8U;
        output[offset + index] = static_cast<std::uint8_t>((value >> shift) & 0xffU);
    }
}

std::uint64_t readBigEndian(
    std::span<const std::uint8_t> input,
    std::size_t offset,
    std::size_t byteCount
)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
    {
        value = (value << 8U) | input[offset + index];
    }
    return value;
}
}

void PacketWriter::writeUInt16(std::uint16_t value)
{
    const auto offset = data_.size();
    data_.resize(offset + 2);
    writeBigEndian(std::span<std::uint8_t>(data_), offset, value, 2);
}

void PacketWriter::writeUInt64(std::uint64_t value)
{
    const auto offset = data_.size();
    data_.resize(offset + 8);
    writeBigEndian(std::span<std::uint8_t>(data_), offset, value, 8);
}

void PacketWriter::writeString(const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::length_error("Protocol strings cannot exceed 65535 bytes.");
    }

    writeUInt16(static_cast<std::uint16_t>(value.size()));
    data_.insert(data_.end(), value.begin(), value.end());
}

const std::vector<std::uint8_t>& PacketWriter::data() const noexcept
{
    return data_;
}

PacketReader::PacketReader(std::span<const std::uint8_t> data) noexcept
    : data_(data)
{
}

void PacketReader::require(std::size_t count) const
{
    if (count > data_.size() - offset_)
    {
        throw std::runtime_error("The packet payload is truncated.");
    }
}

std::uint16_t PacketReader::readUInt16()
{
    require(2);
    const auto value = static_cast<std::uint16_t>(readBigEndian(data_, offset_, 2));
    offset_ += 2;
    return value;
}

std::uint64_t PacketReader::readUInt64()
{
    require(8);
    const auto value = readBigEndian(data_, offset_, 8);
    offset_ += 8;
    return value;
}

std::string PacketReader::readString(std::size_t maximumLength)
{
    const auto length = readUInt16();
    if (length > maximumLength)
    {
        throw std::runtime_error("The packet string exceeds its allowed length.");
    }
    require(length);

    const auto* begin = reinterpret_cast<const char*>(data_.data() + offset_);
    std::string value(begin, begin + length);
    offset_ += length;
    return value;
}

bool PacketReader::empty() const noexcept
{
    return offset_ == data_.size();
}

void sendPacket(
    network::TcpConnection& connection,
    MessageType type,
    std::span<const std::uint8_t> payload
)
{
    if (payload.size() > MAX_PAYLOAD_SIZE)
    {
        throw std::length_error("The packet payload exceeds the protocol limit.");
    }

    std::array<std::uint8_t, HEADER_SIZE> header{};
    writeBigEndian(header, 0, PACKET_MAGIC, 4);
    writeBigEndian(header, 4, PROTOCOL_VERSION, 2);
    writeBigEndian(header, 6, static_cast<std::uint16_t>(type), 2);
    writeBigEndian(header, 8, static_cast<std::uint32_t>(payload.size()), 4);

    connection.sendAll(header);
    connection.sendAll(payload);
}

bool receivePacket(network::TcpConnection& connection, Packet& packet)
{
    std::array<std::uint8_t, HEADER_SIZE> header{};
    if (!connection.receiveAll(header))
    {
        return false;
    }

    const auto magic = readBigEndian(header, 0, 4);
    const auto version = readBigEndian(header, 4, 2);
    const auto type = static_cast<std::uint16_t>(readBigEndian(header, 6, 2));
    const auto payloadSize = readBigEndian(header, 8, 4);

    if (magic != PACKET_MAGIC)
    {
        throw std::runtime_error("The server sent a packet with invalid magic bytes.");
    }
    if (version != PROTOCOL_VERSION)
    {
        throw std::runtime_error("The server uses an unsupported protocol version.");
    }
    if (!isKnownMessageType(type))
    {
        throw std::runtime_error("The server sent an unknown message type.");
    }
    if (payloadSize > MAX_PAYLOAD_SIZE)
    {
        throw std::runtime_error("The server packet exceeds the protocol size limit.");
    }

    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payloadSize));
    if (!payload.empty() && !connection.receiveAll(payload))
    {
        throw std::runtime_error("The server closed the connection during a packet.");
    }

    packet = Packet{static_cast<MessageType>(type), std::move(payload)};
    return true;
}
}
