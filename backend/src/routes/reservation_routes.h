#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "database/database.h"

namespace routes
{

void registerReservationRoutes(
    crow::App<crow::CORSHandler> &app,
    Database &database);

} // namespace routes
