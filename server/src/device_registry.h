#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <random>
#include <string>

class DeviceRegistry
{
public:
    explicit DeviceRegistry(std::filesystem::path filePath);

    std::uint64_t getOrAssignPublicId(const std::string& deviceUuid);
    bool markOnline(std::uint64_t publicId, std::uint64_t connectionId);
    void markOffline(std::uint64_t publicId, std::uint64_t connectionId) noexcept;

private:
    void load();
    std::uint64_t generatePublicId();

    std::filesystem::path filePath_;
    std::mutex mutex_;
    std::map<std::string, std::uint64_t> publicIdsByUuid_;
    std::map<std::uint64_t, std::uint64_t> onlineConnections_;
    std::mt19937_64 randomEngine_;
};
