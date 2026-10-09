#include "d1assist/identity/device_identity.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace d1assist::identity
{
namespace
{
std::string generateUuid()
{
    std::random_device randomDevice;
    std::mt19937_64 engine(randomDevice());
    std::uniform_int_distribution<unsigned int> byteDistribution(0, 255);

    std::array<unsigned int, 16> bytes{};
    for (auto& byte : bytes)
    {
        byte = byteDistribution(engine);
    }

    bytes[6] = (bytes[6] & 0x0fU) | 0x40U;
    bytes[8] = (bytes[8] & 0x3fU) | 0x80U;

    constexpr char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);

    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (index == 4 || index == 6 || index == 8 || index == 10)
        {
            result.push_back('-');
        }

        result.push_back(HEX[bytes[index] >> 4U]);
        result.push_back(HEX[bytes[index] & 0x0fU]);
    }

    return result;
}

std::filesystem::path userDataDirectory()
{
#if defined(_WIN32)
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData == nullptr || *localAppData == '\0')
    {
        throw std::runtime_error("LOCALAPPDATA is not set.");
    }
    return std::filesystem::path(localAppData) / "D1Systems" / "D1Assist";
#else
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome != nullptr && *xdgDataHome != '\0')
    {
        return std::filesystem::path(xdgDataHome) / "d1assist";
    }

    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0')
    {
        throw std::runtime_error("HOME is not set.");
    }
    return std::filesystem::path(home) / ".local" / "share" / "d1assist";
#endif
}
}

void DeviceIdentity::loadOrCreate(const std::filesystem::path& filePath)
{
    filePath_ = filePath;
    std::ifstream input(filePath_);

    if (input)
    {
        std::string loadedUuid;
        std::uint64_t loadedPublicId = 0;
        std::string trailingData;

        if (!std::getline(input, loadedUuid) || !(input >> loadedPublicId) ||
            (input >> trailingData) || !isValidDeviceUuid(loadedUuid))
        {
            throw std::runtime_error("The saved device identity is invalid.");
        }

        deviceUuid_ = std::move(loadedUuid);
        publicId_ = loadedPublicId;
        return;
    }

    if (std::filesystem::exists(filePath_))
    {
        throw std::runtime_error("Could not read the saved device identity.");
    }

    deviceUuid_ = generateUuid();
    publicId_ = 0;
    save();
}

const std::string& DeviceIdentity::deviceUuid() const noexcept
{
    return deviceUuid_;
}

std::uint64_t DeviceIdentity::publicId() const noexcept
{
    return publicId_;
}

void DeviceIdentity::setPublicId(std::uint64_t id)
{
    if (id < 100'000'000ULL || id > 999'999'999ULL)
    {
        throw std::invalid_argument("The public ID must contain nine digits.");
    }

    publicId_ = id;
    save();
}

void DeviceIdentity::save() const
{
    if (filePath_.empty())
    {
        throw std::logic_error("Device identity has not been initialized.");
    }

    const auto parent = filePath_.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    const auto temporaryPath = filePath_.string() + ".tmp";
    {
        std::ofstream output(temporaryPath, std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("Could not open the device identity file for writing.");
        }
        output << deviceUuid_ << '\n' << publicId_ << '\n';
        output.flush();
        if (!output)
        {
            throw std::runtime_error("Could not write the device identity file.");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporaryPath, filePath_, error);
    if (error)
    {
        std::filesystem::remove(temporaryPath);
        throw std::system_error(error, "Could not save the device identity file");
    }
}

bool isValidDeviceUuid(const std::string& value) noexcept
{
    if (value.size() != 36)
    {
        return false;
    }

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (index == 8 || index == 13 || index == 18 || index == 23)
        {
            if (value[index] != '-')
            {
                return false;
            }
            continue;
        }

        const char character = value[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f') ||
              (character >= 'A' && character <= 'F')))
        {
            return false;
        }
    }

    return true;
}

std::filesystem::path defaultIdentityPath()
{
    return userDataDirectory() / "device_identity.dat";
}
}
