#include "main_window.h"

#include "d1assist/auth/password_manager.h"
#include "d1assist/protocol/packet.h"
#include "remote_viewer.h"

#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace
{
QString formatDigits(const std::string& digits)
{
    if (digits.size() != 9)
    {
        return QString::fromStdString(digits);
    }

    return QString::fromStdString(
        digits.substr(0, 3) + " " + digits.substr(3, 3) + " " + digits.substr(6, 3)
    );
}

QString formatPassword(const std::string& password)
{
    if (password.size() != 6)
    {
        return QString::fromStdString(password);
    }

    return QString::fromStdString(password.substr(0, 3) + " " + password.substr(3, 3));
}

std::uint64_t registerDevice(
    d1assist::network::TcpConnection& connection,
    const std::string& deviceUuid
)
{
    d1assist::protocol::PacketWriter request;
    request.writeString(deviceUuid);
    d1assist::protocol::sendPacket(
        connection,
        d1assist::protocol::MessageType::RegisterDevice,
        request.data()
    );

    d1assist::protocol::Packet response{};
    if (!d1assist::protocol::receivePacket(connection, response))
    {
        throw std::runtime_error("O servidor encerrou a conexão durante o registro.");
    }
    if (response.type != d1assist::protocol::MessageType::RegisterDeviceResponse)
    {
        throw std::runtime_error("O servidor respondeu com uma mensagem de registro inesperada.");
    }

    d1assist::protocol::PacketReader responseReader(response.payload);
    const auto status = responseReader.readUInt16();
    const auto publicId = responseReader.readUInt64();
    if (!responseReader.empty())
    {
        throw std::runtime_error("O servidor enviou uma resposta de registro inválida.");
    }
    if (status != 0)
    {
        throw std::runtime_error("Este dispositivo já está conectado ao servidor.");
    }

    return publicId;
}

#ifdef _WIN32
WORD virtualKeyForQtKey(std::uint32_t key)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
    {
        return static_cast<WORD>(key);
    }
    if (key >= static_cast<std::uint32_t>(Qt::Key_F1) &&
        key <= static_cast<std::uint32_t>(Qt::Key_F24))
    {
        return static_cast<WORD>(VK_F1 + key - static_cast<std::uint32_t>(Qt::Key_F1));
    }

    switch (key)
    {
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Alt: return VK_MENU;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_CapsLock: return VK_CAPITAL;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_End: return VK_END;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_Meta: return VK_LWIN;
    case Qt::Key_Menu: return VK_APPS;
    case Qt::Key_NumLock: return VK_NUMLOCK;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    default: return 0;
    }
}

WORD mouseButtonFlag(std::uint16_t button, bool pressed)
{
    switch (button)
    {
    case static_cast<std::uint16_t>(Qt::LeftButton):
        return pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case static_cast<std::uint16_t>(Qt::RightButton):
        return pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case static_cast<std::uint16_t>(Qt::MiddleButton):
        return pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    default:
        return 0;
    }
}
#endif
}

MainWindow::MainWindow(
    std::string serverHost,
    std::uint16_t serverPort,
    std::filesystem::path identityPath
)
    : serverHost_(std::move(serverHost)),
      serverPort_(serverPort)
{
    identity_.loadOrCreate(identityPath);
    d1assist::auth::PasswordManager passwordManager;
    passwordManager.regenerate();
    password_ = passwordManager.password();

    setWindowTitle("D1Assist");
    setMinimumWidth(430);
    setWindowIcon(QApplication::windowIcon());
    setStyleSheet(
        "QMainWindow { background-color: #f3f5f8; }"
        "QWidget { color: #1c2636; font-family: 'Segoe UI'; font-size: 10pt; }"
        "QLabel#title { font-size: 22pt; font-weight: 700; }"
        "QLabel#value { font-size: 19pt; font-weight: 600; color: #1259b3; }"
        "QGroupBox { background: white; border: 1px solid #dce2ea; border-radius: 10px;"
        " margin-top: 12px; padding: 16px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }"
        "QLineEdit { background: white; border: 1px solid #c8d0dc; border-radius: 6px;"
        " padding: 9px; }"
        "QPushButton { background: #1259b3; color: white; border: 0; border-radius: 6px;"
        " padding: 9px 16px; font-weight: 600; }"
        "QPushButton:disabled { background: #aab6c5; color: #f3f5f8; }"
    );

    auto* centralWidget = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(centralWidget);
    pageLayout->setContentsMargins(24, 22, 24, 24);
    pageLayout->setSpacing(14);

    auto* title = new QLabel("D1Assist", centralWidget);
    title->setObjectName("title");
    pageLayout->addWidget(title);

    auto* deviceGroup = new QGroupBox("Este dispositivo", centralWidget);
    auto* deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->setSpacing(8);

    statusLabel_ = new QLabel("Aguardando conexão com o servidor...", deviceGroup);
    statusLabel_->setWordWrap(true);
    deviceLayout->addWidget(statusLabel_);

    auto* idCaption = new QLabel("Seu ID", deviceGroup);
    deviceLayout->addWidget(idCaption);
    publicIdLabel_ = new QLabel(
        identity_.publicId() == 0
            ? "Aguardando registro..."
            : formatDigits(std::to_string(identity_.publicId())),
        deviceGroup
    );
    publicIdLabel_->setObjectName("value");
    deviceLayout->addWidget(publicIdLabel_);

    auto* passwordCaption = new QLabel("Senha temporária", deviceGroup);
    deviceLayout->addWidget(passwordCaption);
    passwordLabel_ = new QLabel(formatPassword(password_), deviceGroup);
    passwordLabel_->setObjectName("value");
    deviceLayout->addWidget(passwordLabel_);

    retryButton_ = new QPushButton("Tentar novamente", deviceGroup);
    retryButton_->hide();
    deviceLayout->addWidget(retryButton_);
    pageLayout->addWidget(deviceGroup);

    auto* sessionGroup = new QGroupBox("Sessão remota", centralWidget);
    auto* sessionLayout = new QVBoxLayout(sessionGroup);
    sessionStatusLabel_ = new QLabel(
        "Nenhuma sessão ativa. O acesso só começa após sua autorização.",
        sessionGroup
    );
    sessionStatusLabel_->setWordWrap(true);
    sessionLayout->addWidget(sessionStatusLabel_);
    endSessionButton_ = new QPushButton("Encerrar sessão", sessionGroup);
    endSessionButton_->setStyleSheet("background: #a62c2c;");
    endSessionButton_->hide();
    sessionLayout->addWidget(endSessionButton_, 0, Qt::AlignRight);
    pageLayout->addWidget(sessionGroup);

    auto* connectGroup = new QGroupBox("Acessar outro computador", centralWidget);
    auto* connectLayout = new QVBoxLayout(connectGroup);
    auto* formLayout = new QFormLayout();
    remoteIdInput_ = new QLineEdit(connectGroup);
    remoteIdInput_->setPlaceholderText("000000000");
    remoteIdInput_->setMaxLength(9);
    remoteIdInput_->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{0,9}"),
        remoteIdInput_
    ));
    remotePasswordInput_ = new QLineEdit(connectGroup);
    remotePasswordInput_->setPlaceholderText("Senha de seis dígitos");
    remotePasswordInput_->setMaxLength(6);
    remotePasswordInput_->setEchoMode(QLineEdit::Password);
    remotePasswordInput_->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{0,6}"),
        remotePasswordInput_
    ));
    formLayout->addRow("ID remoto", remoteIdInput_);
    formLayout->addRow("Senha temporária", remotePasswordInput_);
    connectLayout->addLayout(formLayout);

    connectButton_ = new QPushButton("Solicitar acesso", connectGroup);
    connectButton_->setEnabled(false);
    connectButton_->setToolTip("A conexão só começa depois que o outro computador autorizar.");
    connectLayout->addWidget(connectButton_, 0, Qt::AlignRight);

    auto* securityNotice = new QLabel(
        "Protótipo local: a sessão de teste não é criptografada e funciona somente pelo "
        "servidor em 127.0.0.1. Não use em rede externa.",
        connectGroup
    );
    securityNotice->setWordWrap(true);
    securityNotice->setStyleSheet("color: #8a3b12;");
    connectLayout->addWidget(securityNotice);
    pageLayout->addWidget(connectGroup);
    pageLayout->addStretch();

    auto* footer = new QLabel("Desenvolvido por D1Systems", centralWidget);
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("color: #64748b;");
    pageLayout->addWidget(footer);
    setCentralWidget(centralWidget);

    frameTimer_ = new QTimer(this);
    frameTimer_->setInterval(200);
    connect(frameTimer_, &QTimer::timeout, this, [this] {
        captureAndSendFrame();
    });
    connect(retryButton_, &QPushButton::clicked, this, [this] {
        startRegistration();
    });
    connect(connectButton_, &QPushButton::clicked, this, [this] {
        requestConnection();
    });
    connect(endSessionButton_, &QPushButton::clicked, this, [this] {
        finishSession(true);
    });
}

MainWindow::~MainWindow()
{
    shuttingDown_ = true;
    if (viewer_)
    {
        viewer_->setCloseHandler({});
        viewer_->close();
    }
    if (frameTimer_ != nullptr)
    {
        frameTimer_->stop();
    }
    if (registrationThread_.joinable())
    {
        registrationThread_.join();
    }
    if (connection_)
    {
        connection_->shutdown();
    }
    if (brokerThread_.joinable())
    {
        brokerThread_.join();
    }
    if (connection_)
    {
        connection_->close();
    }
}

void MainWindow::startRegistration()
{
    if (brokerThread_.joinable())
    {
        if (connection_)
        {
            connection_->shutdown();
        }
        brokerThread_.join();
    }
    if (connection_)
    {
        connection_->close();
        connection_.reset();
    }
    if (registrationThread_.joinable())
    {
        registrationThread_.join();
    }

    shuttingDown_ = false;
    statusLabel_->setText(
        QString("Conectando ao servidor %1:%2...")
            .arg(QString::fromStdString(serverHost_))
            .arg(serverPort_)
    );
    statusLabel_->setStyleSheet({});
    retryButton_->hide();
    connectButton_->setEnabled(false);

    const auto host = serverHost_;
    const auto port = serverPort_;
    const auto deviceUuid = identity_.deviceUuid();
    registrationThread_ = std::thread([this, host, port, deviceUuid] {
        try
        {
            auto connection = std::make_shared<d1assist::network::TcpConnection>(
                d1assist::network::TcpConnection::connectTo(host, port)
            );
            const auto publicId = registerDevice(*connection, deviceUuid);

            QMetaObject::invokeMethod(
                this,
                [this, connection = std::move(connection), publicId] {
                    registrationSucceeded(connection, publicId);
                },
                Qt::QueuedConnection
            );
        }
        catch (const std::exception& error)
        {
            const auto message = QString::fromUtf8(error.what());
            QMetaObject::invokeMethod(
                this,
                [this, message] {
                    registrationFailed(message);
                },
                Qt::QueuedConnection
            );
        }
    });
}

void MainWindow::registrationSucceeded(
    std::shared_ptr<d1assist::network::TcpConnection> connection,
    std::uint64_t publicId
)
{
    try
    {
        identity_.setPublicId(publicId);
        connection_ = std::move(connection);
        publicIdLabel_->setText(formatDigits(std::to_string(publicId)));
        statusLabel_->setText("Conectado ao servidor. Este dispositivo está online.");
        statusLabel_->setStyleSheet("color: #16703b; font-weight: 600;");
        connectButton_->setEnabled(true);
        brokerThread_ = std::thread([this, connection = connection_] {
            readBrokerMessages(connection);
        });
    }
    catch (const std::exception& error)
    {
        connection->close();
        registrationFailed(QString::fromUtf8(error.what()));
    }
}

void MainWindow::registrationFailed(const QString& message)
{
    if (activeSessionId_ != 0)
    {
        finishSession(false);
    }
    statusLabel_->setText("Não foi possível manter a conexão: " + message);
    statusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
    connectButton_->setEnabled(false);
    retryButton_->show();
}

void MainWindow::readBrokerMessages(
    std::shared_ptr<d1assist::network::TcpConnection> connection
)
{
    try
    {
        d1assist::protocol::Packet packet{};
        while (!shuttingDown_ && d1assist::protocol::receivePacket(*connection, packet))
        {
            QMetaObject::invokeMethod(
                this,
                [this, packet = std::move(packet)] {
                    handleBrokerMessage(packet);
                },
                Qt::QueuedConnection
            );
        }
        if (!shuttingDown_)
        {
            QMetaObject::invokeMethod(
                this,
                [this] {
                    registrationFailed("O servidor encerrou a conexão.");
                },
                Qt::QueuedConnection
            );
        }
    }
    catch (const std::exception& error)
    {
        if (!shuttingDown_)
        {
            const auto message = QString::fromUtf8(error.what());
            QMetaObject::invokeMethod(
                this,
                [this, message] {
                    registrationFailed(message);
                },
                Qt::QueuedConnection
            );
        }
    }
}

void MainWindow::sendPacket(
    d1assist::protocol::MessageType type,
    const d1assist::protocol::PacketWriter& packet
)
{
    if (!connection_ || !connection_->isOpen())
    {
        throw std::runtime_error("A conexão com o servidor não está ativa.");
    }

    std::lock_guard lock(sendMutex_);
    d1assist::protocol::sendPacket(*connection_, type, packet.data());
}

void MainWindow::requestConnection()
{
    try
    {
        if (pendingRequestId_ != 0 || activeSessionId_ != 0)
        {
            throw std::runtime_error("Encerre a solicitação ou sessão atual antes de iniciar outra.");
        }
        if (remotePasswordInput_->text().size() != 6)
        {
            throw std::invalid_argument("Informe a senha temporária de seis dígitos.");
        }
        const auto remoteId = remoteIdInput_->text().toULongLong();
        if (remoteId < 100'000'000ULL || remoteId > 999'999'999ULL)
        {
            throw std::invalid_argument("Informe um ID remoto de nove dígitos.");
        }
        if (remoteId == identity_.publicId())
        {
            throw std::invalid_argument("Não é possível solicitar acesso ao próprio ID.");
        }

        d1assist::protocol::PacketWriter request;
        request.writeUInt64(remoteId);
        sendPacket(d1assist::protocol::MessageType::ConnectRequest, request);
        connectButton_->setEnabled(false);
        sessionStatusLabel_->setText(
            "Solicitação enviada. O outro computador precisa autorizar antes da autenticação."
        );
        sessionStatusLabel_->setStyleSheet("color: #8a3b12; font-weight: 600;");
    }
    catch (const std::exception& error)
    {
        QMessageBox::warning(this, "Não foi possível solicitar acesso", error.what());
    }
}

void MainWindow::handleBrokerMessage(const d1assist::protocol::Packet& packet)
{
    using d1assist::protocol::MessageType;
    using d1assist::protocol::PacketReader;
    using d1assist::protocol::PacketWriter;

    try
    {
        switch (packet.type)
        {
        case MessageType::IncomingConnection:
        {
            PacketReader reader(packet.payload);
            const auto requestId = reader.readUInt64();
            const auto controllerId = reader.readUInt64();
            if (!reader.empty())
            {
                throw std::runtime_error("O pedido de conexão recebido é inválido.");
            }

            const bool available = pendingRequestId_ == 0 && activeSessionId_ == 0;
            const auto answer = available
                ? QMessageBox::question(
                    this,
                    "Solicitação de suporte remoto",
                    QString("O dispositivo %1 solicita acesso a esta tela. "
                            "Aceitará a solicitação? A senha temporária também será exigida.")
                        .arg(formatDigits(std::to_string(controllerId))),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                )
                : QMessageBox::No;

            PacketWriter response;
            response.writeUInt64(requestId);
            response.writeUInt16(answer == QMessageBox::Yes ? 1 : 0);
            sendPacket(MessageType::ConnectionResponse, response);
            if (answer == QMessageBox::Yes)
            {
                pendingRequestId_ = requestId;
                sessionStatusLabel_->setText(
                    "Solicitação autorizada. Aguardando a validação da senha temporária."
                );
                sessionStatusLabel_->setStyleSheet("color: #8a3b12; font-weight: 600;");
            }
            else
            {
                sessionStatusLabel_->setText("Solicitação de acesso recusada.");
            }
            break;
        }
        case MessageType::ConnectionResponse:
        {
            PacketReader reader(packet.payload);
            const auto requestId = reader.readUInt64();
            const auto accepted = reader.readUInt16();
            if (!reader.empty() || accepted != 1)
            {
                throw std::runtime_error("A resposta de autorização é inválida.");
            }
            pendingRequestId_ = requestId;
            PacketWriter authentication;
            authentication.writeUInt64(requestId);
            authentication.writeString(remotePasswordInput_->text().toStdString());
            sendPacket(MessageType::AuthRequest, authentication);
            sessionStatusLabel_->setText("Permissão concedida. Validando a senha temporária...");
            break;
        }
        case MessageType::ConnectRejected:
        {
            QString reason = "O computador de destino recusou ou não pôde receber a solicitação.";
            if (packet.payload.size() == 10)
            {
                PacketReader reader(packet.payload);
                pendingRequestId_ = reader.readUInt64();
                static_cast<void>(reader.readUInt16());
                if (!reader.empty())
                {
                    throw std::runtime_error("A recusa de conexão recebida é inválida.");
                }
            }
            else
            {
                PacketReader reader(packet.payload);
                reason = QString::fromStdString(reader.readString(256));
                if (!reader.empty())
                {
                    throw std::runtime_error("A recusa de conexão recebida é inválida.");
                }
            }
            pendingRequestId_ = 0;
            connectButton_->setEnabled(true);
            sessionStatusLabel_->setText(reason);
            sessionStatusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
            break;
        }
        case MessageType::AuthRequest:
        {
            PacketReader reader(packet.payload);
            const auto requestId = reader.readUInt64();
            const auto suppliedPassword = reader.readString(6);
            if (!reader.empty())
            {
                throw std::runtime_error("A solicitação de autenticação é inválida.");
            }
            const bool accepted =
                pendingRequestId_ == requestId && suppliedPassword == password_;
            PacketWriter response;
            response.writeUInt64(requestId);
            response.writeUInt16(accepted ? 1 : 0);
            sendPacket(MessageType::AuthResponse, response);
            if (!accepted)
            {
                pendingRequestId_ = 0;
                sessionStatusLabel_->setText("Senha incorreta. Nenhuma sessão foi iniciada.");
                sessionStatusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
            }
            break;
        }
        case MessageType::AuthAccepted:
        {
            PacketReader reader(packet.payload);
            pendingRequestId_ = reader.readUInt64();
            static_cast<void>(reader.readUInt16());
            if (!reader.empty())
            {
                throw std::runtime_error("A resposta de autenticação é inválida.");
            }
            sessionStatusLabel_->setText("Senha aceita. Iniciando sessão remota...");
            break;
        }
        case MessageType::AuthDenied:
        {
            PacketReader reader(packet.payload);
            pendingRequestId_ = reader.readUInt64();
            static_cast<void>(reader.readUInt16());
            if (!reader.empty())
            {
                throw std::runtime_error("A recusa de autenticação é inválida.");
            }
            pendingRequestId_ = 0;
            connectButton_->setEnabled(true);
            sessionStatusLabel_->setText("Senha incorreta. A sessão não foi iniciada.");
            sessionStatusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
            break;
        }
        case MessageType::SessionStart:
        {
            PacketReader reader(packet.payload);
            const auto sessionId = reader.readUInt64();
            const auto role = reader.readUInt16();
            if (!reader.empty() || role > 1)
            {
                throw std::runtime_error("A mensagem de início de sessão é inválida.");
            }
            startSession(sessionId, role == 0);
            break;
        }
        case MessageType::SessionEnd:
        {
            PacketReader reader(packet.payload);
            const auto sessionId = reader.readUInt64();
            if (!reader.empty())
            {
                throw std::runtime_error("A mensagem de encerramento de sessão é inválida.");
            }
            if (activeSessionId_ == sessionId || pendingRequestId_ == sessionId)
            {
                finishSession(false);
                sessionStatusLabel_->setText("A sessão remota foi encerrada.");
            }
            break;
        }
        case MessageType::Frame:
        {
            PacketReader reader(packet.payload);
            const auto sessionId = reader.readUInt64();
            const auto encodedFrame = reader.readBytes();
            if (!reader.empty())
            {
                throw std::runtime_error("O quadro remoto contém dados inesperados.");
            }
            if (sessionId != activeSessionId_ || !controllerRole_)
            {
                break;
            }
            const auto image = QImage::fromData(encodedFrame.data(),
                                                static_cast<qsizetype>(encodedFrame.size()),
                                                "JPEG");
            if (image.isNull() || image.width() > 1280 || image.height() > 720)
            {
                throw std::runtime_error("O quadro recebido é inválido ou excede a resolução permitida.");
            }
            if (viewer_)
            {
                viewer_->displayFrame(image);
            }
            break;
        }
        case MessageType::MouseMove:
        case MessageType::MouseButton:
        case MessageType::MouseWheel:
        case MessageType::KeyDown:
        case MessageType::KeyUp:
            applyRemoteInput(packet);
            break;
        default:
            throw std::runtime_error("O servidor enviou uma mensagem não reconhecida.");
        }
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha na sessão: %1").arg(error.what()));
        sessionStatusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
        finishSession(true);
    }
}

void MainWindow::startSession(std::uint64_t sessionId, bool controller)
{
    if (activeSessionId_ != 0)
    {
        finishSession(true);
    }
    activeSessionId_ = sessionId;
    pendingRequestId_ = 0;
    controllerRole_ = controller;
    connectButton_->setEnabled(false);
    endSessionButton_->show();
    if (controllerRole_)
    {
        sessionStatusLabel_->setText(
            "Sessão autenticada. Use o visualizador para controlar o computador remoto."
        );
        viewer_ = new RemoteViewer(
            [this](std::uint16_t x, std::uint16_t y) {
                sendMouseMove(x, y);
            },
            [this](std::uint16_t button, bool pressed, std::uint16_t x, std::uint16_t y) {
                sendMouseButton(button, pressed, x, y);
            },
            [this](std::int16_t delta, std::uint16_t x, std::uint16_t y) {
                sendMouseWheel(delta, x, y);
            },
            [this](std::uint32_t key, bool pressed) {
                sendKey(key, pressed);
            }
        );
        viewer_->setAttribute(Qt::WA_DeleteOnClose);
        viewer_->setCloseHandler([this] {
            finishSession(true);
        });
        viewer_->show();
    }
    else
    {
        sessionStatusLabel_->setText(
            "Sessão ativa. A tela está sendo compartilhada e o outro usuário pode enviar "
            "movimentos de mouse e teclas. Use Encerrar sessão para parar imediatamente."
        );
        frameTimer_->start();
        captureAndSendFrame();
    }
    sessionStatusLabel_->setStyleSheet("color: #16703b; font-weight: 600;");
}

void MainWindow::finishSession(bool notifyPeer)
{
    if (endingSession_)
    {
        return;
    }
    endingSession_ = true;
    const auto sessionId = activeSessionId_ != 0 ? activeSessionId_ : pendingRequestId_;
    if (activeSessionId_ != 0 && !controllerRole_)
    {
        releaseRemoteInput();
    }
    activeSessionId_ = 0;
    pendingRequestId_ = 0;
    frameTimer_->stop();
    connectButton_->setEnabled(connection_ && connection_->isOpen());
    endSessionButton_->hide();

    if (notifyPeer && sessionId != 0 && connection_ && connection_->isOpen())
    {
        try
        {
            d1assist::protocol::PacketWriter end;
            end.writeUInt64(sessionId);
            sendPacket(d1assist::protocol::MessageType::SessionEnd, end);
        }
        catch (const std::exception& error)
        {
            statusLabel_->setText(QString("A sessão foi encerrada localmente; falha ao avisar "
                                          "o servidor: %1").arg(error.what()));
            statusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
        }
    }

    if (viewer_)
    {
        viewer_->setCloseHandler({});
        viewer_->close();
        viewer_.clear();
    }
    sessionStatusLabel_->setStyleSheet({});
    if (sessionStatusLabel_->text().startsWith("Sessão ativa") ||
        sessionStatusLabel_->text().startsWith("Sessão autenticada"))
    {
        sessionStatusLabel_->setText("Nenhuma sessão ativa. O acesso só começa após sua autorização.");
    }
    endingSession_ = false;
}

void MainWindow::releaseRemoteInput() noexcept
{
#ifdef _WIN32
    for (const auto button : pressedMouseButtons_)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouseButtonFlag(button, false);
        if (input.mi.dwFlags != 0 && SendInput(1, &input, sizeof(input)) != 1)
        {
            std::cerr << "Could not release a remotely held mouse button. Win32 error "
                      << GetLastError() << '\n';
        }
    }
    for (const auto key : pressedKeys_)
    {
        const auto virtualKey = virtualKeyForQtKey(key);
        if (virtualKey == 0)
        {
            continue;
        }
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = virtualKey;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        if (SendInput(1, &input, sizeof(input)) != 1)
        {
            std::cerr << "Could not release a remotely held key. Win32 error "
                      << GetLastError() << '\n';
        }
    }
#endif
    pressedMouseButtons_.clear();
    pressedKeys_.clear();
}

void MainWindow::sendMouseMove(std::uint16_t x, std::uint16_t y)
{
    if (activeSessionId_ == 0 || !controllerRole_)
    {
        return;
    }
    try
    {
        d1assist::protocol::PacketWriter packet;
        packet.writeUInt64(activeSessionId_);
        packet.writeUInt16(x);
        packet.writeUInt16(y);
        sendPacket(d1assist::protocol::MessageType::MouseMove, packet);
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha ao enviar movimento do mouse: %1")
                                         .arg(error.what()));
        finishSession(false);
    }
}

void MainWindow::sendMouseButton(
    std::uint16_t button,
    bool pressed,
    std::uint16_t x,
    std::uint16_t y
)
{
    if (activeSessionId_ == 0 || !controllerRole_)
    {
        return;
    }
    try
    {
        d1assist::protocol::PacketWriter packet;
        packet.writeUInt64(activeSessionId_);
        packet.writeUInt16(button);
        packet.writeUInt16(pressed ? 1 : 0);
        packet.writeUInt16(x);
        packet.writeUInt16(y);
        sendPacket(d1assist::protocol::MessageType::MouseButton, packet);
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha ao enviar botão do mouse: %1")
                                         .arg(error.what()));
        finishSession(false);
    }
}

void MainWindow::sendMouseWheel(std::int16_t delta, std::uint16_t x, std::uint16_t y)
{
    if (activeSessionId_ == 0 || !controllerRole_)
    {
        return;
    }
    try
    {
        d1assist::protocol::PacketWriter packet;
        packet.writeUInt64(activeSessionId_);
        packet.writeUInt16(static_cast<std::uint16_t>(delta));
        packet.writeUInt16(x);
        packet.writeUInt16(y);
        sendPacket(d1assist::protocol::MessageType::MouseWheel, packet);
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha ao enviar a roda do mouse: %1")
                                         .arg(error.what()));
        finishSession(false);
    }
}

void MainWindow::sendKey(std::uint32_t key, bool pressed)
{
    if (activeSessionId_ == 0 || !controllerRole_)
    {
        return;
    }
    try
    {
        d1assist::protocol::PacketWriter packet;
        packet.writeUInt64(activeSessionId_);
        packet.writeUInt32(key);
        sendPacket(pressed
                       ? d1assist::protocol::MessageType::KeyDown
                       : d1assist::protocol::MessageType::KeyUp,
                   packet);
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha ao enviar tecla: %1").arg(error.what()));
        finishSession(false);
    }
}

void MainWindow::captureAndSendFrame()
{
    if (activeSessionId_ == 0 || controllerRole_)
    {
        return;
    }
    auto* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr)
    {
        sessionStatusLabel_->setText("Não foi possível localizar a tela para compartilhamento.");
        finishSession(true);
        return;
    }

    try
    {
        auto frame = screen->grabWindow(0).toImage();
        if (frame.isNull())
        {
            throw std::runtime_error("A captura da tela retornou uma imagem vazia.");
        }
        frame = frame.scaled(
            1280,
            720,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        QByteArray encoded;
        QBuffer buffer(&encoded);
        if (!buffer.open(QIODevice::WriteOnly) || !frame.save(&buffer, "JPEG", 55))
        {
            throw std::runtime_error("Não foi possível codificar a captura de tela.");
        }
        if (encoded.size() > 1'048'560)
        {
            frame = frame.scaled(960, 540, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            encoded.clear();
            if (!buffer.seek(0))
            {
                throw std::runtime_error("Não foi possível reiniciar a codificação do quadro.");
            }
            if (!frame.save(&buffer, "JPEG", 35) || encoded.size() > 1'048'560)
            {
                throw std::runtime_error("A captura excede o limite de tamanho do protocolo.");
            }
        }

        const auto* begin = reinterpret_cast<const std::uint8_t*>(encoded.constData());
        const std::span<const std::uint8_t> bytes(begin, static_cast<std::size_t>(encoded.size()));
        d1assist::protocol::PacketWriter packet;
        packet.writeUInt64(activeSessionId_);
        packet.writeBytes(bytes);
        sendPacket(d1assist::protocol::MessageType::Frame, packet);
    }
    catch (const std::exception& error)
    {
        sessionStatusLabel_->setText(QString("Falha ao compartilhar a tela: %1")
                                         .arg(error.what()));
        sessionStatusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
        finishSession(true);
    }
}

void MainWindow::applyRemoteInput(const d1assist::protocol::Packet& packet)
{
    using d1assist::protocol::MessageType;
    d1assist::protocol::PacketReader reader(packet.payload);
    const auto sessionId = reader.readUInt64();
    if (sessionId != activeSessionId_ || controllerRole_)
    {
        return;
    }

#ifdef _WIN32
    INPUT input{};
    if (packet.type == MessageType::MouseMove)
    {
        const auto x = reader.readUInt16();
        const auto y = reader.readUInt16();
        if (!reader.empty())
        {
            throw std::runtime_error("O movimento remoto do mouse contém dados inválidos.");
        }
        input.type = INPUT_MOUSE;
        input.mi.dx = x;
        input.mi.dy = y;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        if (SendInput(1, &input, sizeof(input)) != 1)
        {
            throw std::runtime_error("O Windows não aceitou o movimento remoto do mouse.");
        }
        return;
    }
    if (packet.type == MessageType::MouseButton)
    {
        const auto button = reader.readUInt16();
        const auto pressed = reader.readUInt16();
        const auto x = reader.readUInt16();
        const auto y = reader.readUInt16();
        if (!reader.empty() || pressed > 1)
        {
            throw std::runtime_error("O botão remoto do mouse contém dados inválidos.");
        }
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dx = x;
        inputs[0].mi.dy = y;
        inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = mouseButtonFlag(button, pressed == 1);
        if (inputs[1].mi.dwFlags == 0 || SendInput(2, inputs, sizeof(INPUT)) != 2)
        {
            throw std::runtime_error("O Windows não aceitou o botão remoto do mouse.");
        }
        if (pressed == 1)
        {
            pressedMouseButtons_.insert(button);
        }
        else
        {
            pressedMouseButtons_.erase(button);
        }
        return;
    }
    if (packet.type == MessageType::MouseWheel)
    {
        const auto delta = static_cast<std::int16_t>(reader.readUInt16());
        const auto x = reader.readUInt16();
        const auto y = reader.readUInt16();
        if (!reader.empty())
        {
            throw std::runtime_error("A roda remota do mouse contém dados inválidos.");
        }
        input.type = INPUT_MOUSE;
        input.mi.dx = x;
        input.mi.dy = y;
        input.mi.mouseData = static_cast<DWORD>(static_cast<LONG>(delta));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_WHEEL;
        if (SendInput(1, &input, sizeof(input)) != 1)
        {
            throw std::runtime_error("O Windows não aceitou a roda remota do mouse.");
        }
        return;
    }

    const auto key = reader.readUInt32();
    if (!reader.empty())
    {
        throw std::runtime_error("O evento de teclado remoto contém dados inválidos.");
    }
    const auto virtualKey = virtualKeyForQtKey(key);
    if (virtualKey == 0)
    {
        return;
    }
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.dwFlags = packet.type == MessageType::KeyUp ? KEYEVENTF_KEYUP : 0;
    if (SendInput(1, &input, sizeof(input)) != 1)
    {
        throw std::runtime_error("O Windows não aceitou a tecla remota.");
    }
    if (packet.type == MessageType::KeyDown)
    {
        pressedKeys_.insert(key);
    }
    else
    {
        pressedKeys_.erase(key);
    }
#else
    static_cast<void>(reader);
    throw std::runtime_error("O controle remoto de entrada está disponível somente no Windows.");
#endif
}
