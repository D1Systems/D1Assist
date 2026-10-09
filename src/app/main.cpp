#include "main_window.h"

#include "d1assist/identity/device_identity.h"

#include <QApplication>
#include <QMessageBox>
#include <QStringList>

#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace
{
std::uint16_t parsePort(const QString& value)
{
    const auto text = value.toStdString();
    unsigned int port = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        port == 0 || port > 65'535)
    {
        throw std::invalid_argument("A porta deve ser um número entre 1 e 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

void showUsage()
{
    QMessageBox::information(
        nullptr,
        "Uso do D1Assist",
        "Parâmetros opcionais: [endereço do servidor] [porta] [arquivo de identidade]\n"
        "Padrões: 127.0.0.1, porta 5577 e diretório de dados do usuário."
    );
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("D1Assist");
    QCoreApplication::setOrganizationName("D1Systems");

    try
    {
        const auto arguments = QCoreApplication::arguments();
        if (arguments.size() > 4)
        {
            showUsage();
            return 2;
        }

        const std::string host = arguments.size() >= 2
            ? arguments[1].toStdString()
            : "127.0.0.1";
        const std::uint16_t port = arguments.size() >= 3
            ? parsePort(arguments[2])
            : 5577;
        const auto identityPath = arguments.size() >= 4
            ? std::filesystem::path(arguments[3].toStdString())
            : d1assist::identity::defaultIdentityPath();

        MainWindow window(host, port, identityPath);
        window.show();
        window.startRegistration();
        return application.exec();
    }
    catch (const std::exception& error)
    {
        QMessageBox::critical(
            nullptr,
            "Erro ao iniciar o D1Assist",
            QString::fromUtf8(error.what())
        );
        return 1;
    }
}
