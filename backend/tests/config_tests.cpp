#include "../src/config/config.h"

#include <cassert>
#include <cstdlib>
#include <string>

namespace
{
void clearConfiguration()
{
    _putenv_s("PARKING_DB_HOST", "");
    _putenv_s("PARKING_DB_PORT", "");
    _putenv_s("PARKING_DB_NAME", "");
    _putenv_s("PARKING_DB_USER", "");
    _putenv_s("PARKING_DB_PASSWORD", "");
    _putenv_s("PARKING_SERVER_PORT", "");
}
}

int main()
{
    clearConfiguration();
    _putenv_s("PARKING_DB_PASSWORD", "test-only-password");

    const BackendConfig defaults = BackendConfig::fromEnvironment();
    assert(defaults.database_host == "localhost");
    assert(defaults.database_port == "5432");
    assert(defaults.database_name == "vehicle_parking");
    assert(defaults.database_user == "postgres");
    assert(defaults.server_port == 3000);

    _putenv_s("PARKING_SERVER_PORT", "3100");
    const BackendConfig custom_port = BackendConfig::fromEnvironment();
    assert(custom_port.server_port == 3100);

    _putenv_s("PARKING_SERVER_PORT", "31x0");
    bool invalid_port_rejected = false;
    try
    {
        BackendConfig::fromEnvironment();
    }
    catch (const std::exception &exception)
    {
        invalid_port_rejected = std::string(exception.what()).find("PARKING_SERVER_PORT") != std::string::npos;
    }
    assert(invalid_port_rejected);

    clearConfiguration();
    bool missing_password_rejected = false;
    try
    {
        BackendConfig::fromEnvironment();
    }
    catch (const std::exception &exception)
    {
        missing_password_rejected = std::string(exception.what()).find("PARKING_DB_PASSWORD") != std::string::npos;
    }
    assert(missing_password_rejected);

    return 0;
}
