#pragma once

#include "d1assist/identity/device_identity.h"
#include "d1assist/network/tcp_connection.h"

#include <QMainWindow>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

class QLabel;
class QLineEdit;
class QPushButton;

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

    std::string serverHost_;
    std::uint16_t serverPort_;
    d1assist::identity::DeviceIdentity identity_;
    std::string password_;
    std::shared_ptr<d1assist::network::TcpConnection> connection_;
    std::thread registrationThread_;

    QLabel* statusLabel_ = nullptr;
    QLabel* publicIdLabel_ = nullptr;
    QLabel* passwordLabel_ = nullptr;
    QPushButton* retryButton_ = nullptr;
};
