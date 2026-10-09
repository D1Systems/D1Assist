#include "main_window.h"

#include "d1assist/auth/password_manager.h"
#include "d1assist/protocol/packet.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>
#include <stdexcept>

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
        throw std::runtime_error("The server disconnected before completing registration.");
    }
    if (response.type != d1assist::protocol::MessageType::RegisterDeviceResponse)
    {
        throw std::runtime_error("The server returned an unexpected registration message.");
    }

    d1assist::protocol::PacketReader responseReader(response.payload);
    const auto status = responseReader.readUInt16();
    const auto publicId = responseReader.readUInt64();
    if (!responseReader.empty())
    {
        throw std::runtime_error("The server returned an invalid registration response.");
    }
    if (status != 0)
    {
        throw std::runtime_error("This device is already connected to the server.");
    }

    return publicId;
}
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

    auto* connectGroup = new QGroupBox("Acessar outro computador", centralWidget);
    auto* connectLayout = new QVBoxLayout(connectGroup);
    auto* formLayout = new QFormLayout();
    auto* remoteIdInput = new QLineEdit(connectGroup);
    remoteIdInput->setPlaceholderText("000 000 000");
    remoteIdInput->setMaxLength(11);
    auto* remotePasswordInput = new QLineEdit(connectGroup);
    remotePasswordInput->setPlaceholderText("Senha de seis dígitos");
    remotePasswordInput->setMaxLength(6);
    formLayout->addRow("ID remoto", remoteIdInput);
    formLayout->addRow("Senha", remotePasswordInput);
    connectLayout->addLayout(formLayout);

    auto* connectButton = new QPushButton("Conectar", connectGroup);
    connectButton->setEnabled(false);
    connectButton->setToolTip(
        "A solicitação de conexão será habilitada quando a autenticação e o consentimento "
        "estiverem implementados."
    );
    connectLayout->addWidget(connectButton, 0, Qt::AlignRight);

    auto* nextStepNotice = new QLabel(
        "A conexão remota será habilitada após implementarmos a autenticação e o consentimento.",
        connectGroup
    );
    nextStepNotice->setWordWrap(true);
    nextStepNotice->setStyleSheet("color: #64748b;");
    connectLayout->addWidget(nextStepNotice);
    pageLayout->addWidget(connectGroup);
    pageLayout->addStretch();

    setCentralWidget(centralWidget);

    connect(retryButton_, &QPushButton::clicked, this, [this] {
        startRegistration();
    });
}

MainWindow::~MainWindow()
{
    if (registrationThread_.joinable())
    {
        registrationThread_.join();
    }
}

void MainWindow::startRegistration()
{
    if (registrationThread_.joinable())
    {
        registrationThread_.join();
    }

    statusLabel_->setText(
        QString("Conectando ao servidor %1:%2...")
            .arg(QString::fromStdString(serverHost_))
            .arg(serverPort_)
    );
    retryButton_->hide();

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
    }
    catch (const std::exception& error)
    {
        connection->close();
        registrationFailed(QString::fromUtf8(error.what()));
    }
}

void MainWindow::registrationFailed(const QString& message)
{
    statusLabel_->setText("Não foi possível registrar o dispositivo: " + message);
    statusLabel_->setStyleSheet("color: #a62c2c; font-weight: 600;");
    retryButton_->show();
}
