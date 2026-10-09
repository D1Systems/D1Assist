#pragma once

#include <string>

namespace d1assist::auth
{
class PasswordManager
{
public:
    void regenerate();
    std::string password() const;
    bool validate(const std::string& password) const;

private:
    std::string password_;
};
}
