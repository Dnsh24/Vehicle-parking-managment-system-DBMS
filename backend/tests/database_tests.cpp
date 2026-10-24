#include "../src/config/config.h"
#include "../src/database/database.h"

#include <cassert>
#include <cstdlib>

int main()
{
    _putenv_s("PARKING_DB_HOST", "localhost");
    _putenv_s("PARKING_DB_PORT", "5432");
    _putenv_s("PARKING_DB_NAME", "vehicle_parking");
    _putenv_s("PARKING_DB_USER", "postgres");
    _putenv_s("PARKING_DB_PASSWORD", "test-only-invalid-password");

    const BackendConfig config = BackendConfig::fromEnvironment();
    Database database(config);
    const DatabaseCheckResult result = database.checkConnection();

    assert(!result.connected);
    assert(!result.error_message.empty());
    return 0;
}
