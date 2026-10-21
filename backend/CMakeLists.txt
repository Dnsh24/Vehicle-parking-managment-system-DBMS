cmake_minimum_required(VERSION 3.20)

project(parking_backend LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

include(FetchContent)

FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)

FetchContent_MakeAvailable(Crow)

find_package(libpqxx CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(parking_backend
    src/main.cpp
    src/auth/auth.cpp
    src/auth/auth.h
    src/api/parking_responses.h
    src/api/vehicle_responses.h
    src/api/admin_responses.h
    src/config/config.cpp
    src/database/database.cpp
    src/validation/validation.cpp
    src/routes/route_helpers.cpp
    src/routes/route_helpers.h
    src/routes/auth_routes.cpp
    src/routes/auth_routes.h
    src/routes/parking_routes.cpp
    src/routes/parking_routes.h
    src/routes/reservation_routes.cpp
    src/routes/reservation_routes.h
    src/routes/vehicle_routes.cpp
    src/routes/vehicle_routes.h
    src/routes/admin_routes.cpp
    src/routes/admin_routes.h
)

target_include_directories(parking_backend
    PRIVATE
    src
)

target_link_libraries(parking_backend
    PRIVATE
    Crow::Crow
    libpqxx::pqxx
    OpenSSL::Crypto
)

if(BUILD_TESTING)
    add_executable(validation_tests
        tests/validation_tests.cpp
        src/validation/validation.cpp
    )

    add_test(NAME validation_tests COMMAND validation_tests)

    add_executable(config_tests
        tests/config_tests.cpp
        src/config/config.cpp
    )

    add_test(NAME config_tests COMMAND config_tests)

    add_executable(api_tests
        tests/api_tests.cpp
    )

    target_link_libraries(api_tests
        PRIVATE
        Crow::Crow
    )
    if(WIN32)
        target_link_libraries(api_tests PRIVATE ws2_32)
    endif()

    add_test(NAME api_tests COMMAND api_tests)

    add_executable(database_tests
        tests/database_tests.cpp
        src/config/config.cpp
        src/database/database.cpp
    )

    target_link_libraries(database_tests
        PRIVATE
        libpqxx::pqxx
    )

    add_test(NAME database_tests COMMAND database_tests)

    add_executable(auth_tests
        tests/auth_tests.cpp
        src/auth/auth.cpp
    )

    target_link_libraries(auth_tests
        PRIVATE
        OpenSSL::Crypto
    )

    add_test(NAME auth_tests COMMAND auth_tests)

    add_executable(admin_tests
        tests/admin_tests.cpp
        src/api/admin_responses.h
    )

    target_link_libraries(admin_tests
        PRIVATE
        Crow::Crow
    )

    add_test(NAME admin_tests COMMAND admin_tests)
endif()

