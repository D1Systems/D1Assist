#pragma once

#include "d1assist/identity/device_identity.h"
#include "d1assist/network/tcp_connection.h"

#include <QMainWindow>
#include <QPointer>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class RemoteViewer;
namespace d1assist::protocol
{
enum class MessageType : std::uint16_t;
struct Packet;
class PacketWriter;
}

class MainWindow : public QMainWindow
{
public:
    MainWindow(
        std::string serverHost,
        std::uint16_t serverPort,
        std::filesystem::path identityPath
    );
    ~MainWindow() override;

    void startRegistration();

private:
    void registrationSucceeded(
        std::shared_ptr<d1assist::network::TcpConnection> connection,
        std::uint64_t publicId
    );
    void registrationFailed(const QString& message);
    void readBrokerMessages(std::shared_ptr<d1assist::network::TcpConnection> connection);
    void handleBrokerMessage(const d1assist::protocol::Packet& packet);
    void requestConnection();
    void sendPacket(
        d1assist::protocol::MessageType type,
        const d1assist::protocol::PacketWriter& packet
    );
    void sendMouseMove(std::uint16_t x, std::uint16_t y);
    void sendMouseButton(std::uint16_t button, bool pressed, std::uint16_t x, std::uint16_t y);
    void sendMouseWheel(std::int16_t delta, std::uint16_t x, std::uint16_t y);
    void sendKey(std::uint32_t key, bool pressed);
    void sendSessionEnd();
    void startSession(std::uint64_t sessionId, bool controller);
    void finishSession(bool notifyPeer);
    void releaseRemoteInput() noexcept;
    void captureAndSendFrame();
    void applyRemoteInput(const d1assist::protocol::Packet& packet);

    std::string serverHost_;
    std::uint16_t serverPort_;
    d1assist::identity::DeviceIdentity identity_;
    std::string password_;
    std::shared_ptr<d1assist::network::TcpConnection> connection_;
    std::thread registrationThread_;
    std::thread brokerThread_;
    std::atomic<bool> shuttingDown_{false};
    std::mutex sendMutex_;
    std::uint64_t pendingRequestId_ = 0;
    std::uint64_t activeSessionId_ = 0;
    bool controllerRole_ = false;
    bool endingSession_ = false;
    QPointer<RemoteViewer> viewer_;
    std::set<std::uint16_t> pressedMouseButtons_;
    std::set<std::uint32_t> pressedKeys_;

    QLabel* statusLabel_ = nullptr;
    QLabel* sessionStatusLabel_ = nullptr;
    QLabel* publicIdLabel_ = nullptr;
    QLabel* passwordLabel_ = nullptr;
    QPushButton* retryButton_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* endSessionButton_ = nullptr;
    QLineEdit* remoteIdInput_ = nullptr;
    QLineEdit* remotePasswordInput_ = nullptr;
    QTimer* frameTimer_ = nullptr;
};
