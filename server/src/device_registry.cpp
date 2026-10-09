#include "device_registry.h"

#include "d1assist/identity/device_identity.h"

#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>

DeviceRegistry::DeviceRegistry(std::filesystem::path filePath)
    : filePath_(std::move(filePath)),
      randomEngine_(std::random_device{}())
{
    load();
}

std::uint64_t DeviceRegistry::getOrAssignPublicId(const std::string& deviceUuid)
{
    if (!d1assist::identity::isValidDeviceUuid(deviceUuid))
    {
        throw std::invalid_argument("Device UUID is malformed.");
    }

    std::lock_guard lock(mutex_);
    const auto existing = publicIdsByUuid_.find(deviceUuid);
    if (existing != publicIdsByUuid_.end())
    {
        return existing->second;
    }

    const auto publicId = generatePublicId();
    const auto parent = filePath_.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(filePath_, std::ios::app);
    if (!output)
    {
        throw std::runtime_error("Could not open the device registry for writing.");
    }
    output << deviceUuid << ' ' << publicId << '\n';
    output.flush();
    if (!output)
    {
        throw std::runtime_error("Could not persist the device registration.");
    }

    publicIdsByUuid_.emplace(deviceUuid, publicId);
    return publicId;
}

bool DeviceRegistry::markOnline(std::uint64_t publicId, std::uint64_t connectionId)
{
    std::lock_guard lock(mutex_);
    return onlineConnections_.emplace(publicId, connectionId).second;
}

void DeviceRegistry::markOffline(std::uint64_t publicId, std::uint64_t connectionId) noexcept
{
    std::lock_guard lock(mutex_);
    const auto connection = onlineConnections_.find(publicId);
    if (connection != onlineConnections_.end() && connection->second == connectionId)
    {
        onlineConnections_.erase(connection);
    }
}

void DeviceRegistry::load()
{
    std::ifstream input(filePath_);
    if (!input)
    {
        if (std::filesystem::exists(filePath_))
        {
            throw std::runtime_error("Could not read the device registry.");
        }
        return;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (line.empty())
        {
            continue;
        }

        std::istringstream row(line);
        std::string uuid;
        std::uint64_t publicId = 0;
        std::string trailingData;
        if (!(row >> uuid >> publicId) || (row >> trailingData) ||
            !d1assist::identity::isValidDeviceUuid(uuid) ||
            publicId < 100'000'000ULL || publicId > 999'999'999ULL)
        {
            throw std::runtime_error(
                "Invalid device registry entry at line " + std::to_string(lineNumber) + "."
            );
        }

        const auto [uuidEntry, insertedUuid] = publicIdsByUuid_.emplace(uuid, publicId);
        if (!insertedUuid && uuidEntry->second != publicId)
        {
            throw std::runtime_error("A device UUID has conflicting IDs in the registry.");
        }
        for (const auto& [otherUuid, otherId] : publicIdsByUuid_)
        {
            if (otherUuid != uuid && otherId == publicId)
            {
                throw std::runtime_error("Two devices share a public ID in the registry.");
            }
        }
    }

    if (!input.eof())
    {
        throw std::runtime_error("Could not finish reading the device registry.");
    }
}

std::uint64_t DeviceRegistry::generatePublicId()
{
    std::uniform_int_distribution<std::uint64_t> distribution(100'000'000ULL, 999'999'999ULL);
    for (std::size_t attempt = 0; attempt < 1'000'000; ++attempt)
    {
        const auto candidate = distribution(randomEngine_);
        bool used = false;
        for (const auto& [uuid, publicId] : publicIdsByUuid_)
        {
            static_cast<void>(uuid);
            if (publicId == candidate)
            {
                used = true;
                break;
            }
        }
        if (!used)
        {
            return candidate;
        }
    }

    throw std::runtime_error("Could not allocate a unique public ID.");
}
