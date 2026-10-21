#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "database/database.h"

#ifdef DELETE
#undef DELETE
#endif

namespace routes
{

void registerVehicleRoutes(crow::App<crow::CORSHandler> &app, Database &database);

} // namespace routes

