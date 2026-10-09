#pragma once

#include <cstdint>

namespace d1assist::protocol
{
enum class MessageType : std::uint16_t
{
    Hello = 1,
    HelloAck = 2,
    RegisterDevice = 10,
    RegisterDeviceResponse = 11,
    ConnectRequest = 20,
    IncomingConnection = 21,
    ConnectRejected = 22,
    AuthRequest = 30,
    AuthResponse = 31,
    AuthAccepted = 32,
    AuthDenied = 33,
    SessionStart = 40,
    SessionEnd = 41,
    Frame = 100,
    MouseMove = 101,
    MouseButton = 102,
    MouseWheel = 103,
    KeyDown = 110,
    KeyUp = 111,
    Clipboard = 120,
    FileTransferStart = 130,
    FileTransferData = 131,
    FileTransferEnd = 132
};
}
