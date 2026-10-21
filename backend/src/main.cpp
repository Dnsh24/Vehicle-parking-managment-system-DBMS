#include "crow.h"
#include "crow/middlewares/cors.h"
#include "config/config.h"
#include "database/database.h"
#include "routes/admin_routes.h"
#include "routes/auth_routes.h"
#include "routes/parking_routes.h"
#include "routes/reservation_routes.h"
#include "routes/vehicle_routes.h"

#include <cstdlib>
#include <iostream>

#ifdef DELETE
#undef DELETE
#endif

int main()
{
#ifdef _WIN32
    if (std::getenv("PARKING_DB_PASSWORD") == nullptr || std::getenv("PARKING_DB_PASSWORD")[0] == '\0')
    {
        const char *pgpass = std::getenv("PGPASSWORD");
        if (pgpass != nullptr && pgpass[0] != '\0')
        {
            _putenv_s("PARKING_DB_PASSWORD", pgpass);
        }
        else
        {
            _putenv_s("PARKING_DB_PASSWORD", "@24Dinesh");
        }
    }
#else
    if (std::getenv("PARKING_DB_PASSWORD") == nullptr || std::getenv("PARKING_DB_PASSWORD")[0] == '\0')
    {
        const char *pgpass = std::getenv("PGPASSWORD");
        if (pgpass != nullptr && pgpass[0] != '\0')
        {
            setenv("PARKING_DB_PASSWORD", pgpass, 1);
        }
        else
        {
            setenv("PARKING_DB_PASSWORD", "@24Dinesh", 1);
        }
    }
#endif

    BackendConfig config;
    try
    {
        config = BackendConfig::fromEnvironment();
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Configuration error: " << exception.what() << std::endl;
        return 1;
    }

    crow::App<crow::CORSHandler> app;

    auto &cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type, Accept, Origin, Authorization")
        .methods(
            crow::HTTPMethod::GET,
            crow::HTTPMethod::POST,
            crow::HTTPMethod::PUT,
            crow::HTTPMethod::DELETE,
            crow::HTTPMethod::OPTIONS)
        .origin("*");

    Database database(config);
    const DatabaseCheckResult database_check = database.checkConnection();

    if (database_check.connected)
    {
        std::cout << "Database check successful!" << std::endl;
    }
    else
    {
        std::cerr << "Database connection failed: "
                  << database_check.error_message << std::endl;
    }

    // Register modular route handlers
    routes::registerParkingRoutes(app, database, database_check, config.database_name);
    routes::registerAuthRoutes(app, database);
    routes::registerVehicleRoutes(app, database);
    routes::registerReservationRoutes(app, database);
    routes::registerAdminRoutes(app, database);

    std::cout << "Server running on http://localhost:" << config.server_port << std::endl;

    app.port(config.server_port).multithreaded().run();

    return 0;
}
