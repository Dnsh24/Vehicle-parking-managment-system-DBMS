#pragma once

#include "crow.h"
#include "api/errors.h"
#include "database/database.h"

#ifdef DELETE
#undef DELETE
#endif

#include <cstdint>
#include <optional>
#include <string>

namespace routes
{

struct AuthenticatedUser
{
    std::int64_t user_id;
    std::string name;
    std::string email;
    std::string phone;
    std::string role;
};

std::string lowerEmail(std::string email);
std::string normalizeRegistration(std::string registration);

std::string jsonString(const crow::json::rvalue &body, const char *key);
std::optional<bool> jsonBoolean(const crow::json::rvalue &body, const char *key);
std::optional<std::int64_t> jsonInteger(const crow::json::rvalue &body, const char *key);
std::optional<double> jsonNumber(const crow::json::rvalue &body, const char *key);

crow::response unauthorized();
crow::response forbidden();
crow::json::wvalue userJson(const AuthenticatedUser &user);

std::optional<AuthenticatedUser> findAuthenticatedUser(
    const Database &database,
    const crow::request &request);

std::optional<AuthenticatedUser> findAuthenticatedAdmin(
    const Database &database,
    const crow::request &request);

std::string createSession(const Database &database, std::int64_t user_id);

} // namespace routes

