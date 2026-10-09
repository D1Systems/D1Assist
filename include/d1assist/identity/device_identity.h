#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace d1assist::identity
{
class DeviceIdentity
{
public:
    void loadOrCreate(const std::filesystem::path& filePath);

    const std::string& deviceUuid() const noexcept;
    std::uint64_t publicId() const noexcept;
    void setPublicId(std::uint64_t id);

private:
    void save() const;

    std::filesystem::path filePath_;
    std::string deviceUuid_;
    std::uint64_t publicId_ = 0;
};

bool isValidDeviceUuid(const std::string& value) noexcept;
std::filesystem::path defaultIdentityPath();
}
