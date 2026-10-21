#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "database/database.h"

#ifdef DELETE
#undef DELETE
#endif

#include <string>

namespace routes
{

void registerParkingRoutes(
    crow::App<crow::CORSHandler> &app,
    Database &database,
    const DatabaseCheckResult &database_check,
    const std::string &database_name);

} // namespace routes

