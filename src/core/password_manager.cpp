#include "d1assist/auth/password_manager.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace d1assist::auth
{
void PasswordManager::regenerate()
{
    std::random_device randomDevice;
    std::mt19937 engine(randomDevice());
    std::uniform_int_distribution<unsigned int> distribution(0, 999'999);

    std::ostringstream formatted;
    formatted << std::setw(6) << std::setfill('0') << distribution(engine);
    password_ = formatted.str();
}

std::string PasswordManager::password() const
{
    return password_;
}

bool PasswordManager::validate(const std::string& password) const
{
    return !password_.empty() && password == password_;
}
}
