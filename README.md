# Vehicle Parking Management System

A college project for managing vehicle parking through an HTML/CSS/JavaScript
frontend, a C++ backend using Crow, and a PostgreSQL database.

## Current development status

| Component | Status |
| --- | --- |
| Frontend | Authentication, parking availability, and vehicle parking/checkout UI implemented. |
| PostgreSQL | Schema implemented with 14 core tables, constraints, and indexes. See [database.md](database.md). |
| C++ backend | Crow server with PostgreSQL access, bearer authentication, parking availability, vehicle CRUD, and direct parking assignments. |
| REST API | Health, authentication, profile, parking availability, and vehicle-management endpoints are implemented. |
| Frontend-backend integration | Browser `fetch` integration targets `http://localhost:3000`. |

The database schema does not by itself mean that the corresponding user-facing
features or CRUD operations are implemented.

## Technology stack

- HTML, CSS, and JavaScript frontend
- C++20 backend
- Crow, fetched by CMake
- PostgreSQL
- libpqxx for C++ to PostgreSQL access
- CMake and Ninja-compatible build configuration
- JSON responses from the Crow routes

## Planned architecture

The intended architecture is:

```text
HTML/CSS/JavaScript frontend
            |
        HTTP/JSON
            v
       C++ Crow backend
            |
          libpqxx
            v
       PostgreSQL database
```

Authentication, profiles, parking availability, and vehicle management with
manual slot assignment and checkout are implemented. Reservations, payments,
QR processing, and the remaining application APIs are planned rather than
complete. The Phase 3 schema migration must be applied to an existing database
before the updated vehicle parking routes can run.

## Implemented features

### Frontend

- Supports registration, login, and logout.
- Loads parking-area and slot availability.
- Adds and edits vehicles, shows compatible available slots, and parks a
  selected vehicle until checkout.
- Uses bearer-authenticated requests to the backend at port 3000.

### Backend

- Creates a Crow application with CORS middleware.
- Attempts a PostgreSQL connection to the `parking_management` database.
- Executes `SELECT 1` when the connection is established.
- Provides bearer-session authentication and profile operations.
- Loads parking areas and slots from PostgreSQL.
- Provides owner-scoped vehicle CRUD, compatible-slot selection, and checkout.
- Atomically claims/releases parking slots and persists assignment history.

### Database

- Defines the 14 core tables documented in [database.md](database.md).
- Uses `pgcrypto` and `gen_random_uuid()` for QR token generation.
- Defines primary keys, foreign keys, checks, unique constraints, and indexes.

## Planned features

The following are described in [planning.md](planning.md) but are not yet
implemented as complete application functionality:

- Parking-area and parking-slot management
- Reservation-based availability and slot allocation
- Reservations and reservation expiry
- QR-based check-in and vehicle entry
- Parking sessions, exit processing, and overstay calculation
- Fee calculation, payments, and digital receipts
- Notifications, history, administration, and reporting

## Actual project structure

```text
.
├── README.md
├── planning.md
├── API.md
├── OOPS.md
├── database.md
├── backend
│   ├── CMakeLists.txt
│   └── src
│       ├── api
│       │   └── errors.h
│       ├── config
│       │   ├── config.cpp
│       │   └── config.h
│       ├── database
│       │   ├── database.cpp
│       │   └── database.h
│       ├── main.cpp
│       └── validation
│           ├── validation.cpp
│           └── validation.h
│   └── tests
│       ├── api_tests.cpp
├── config_tests.cpp
├── database_tests.cpp
│       └── validation_tests.cpp
├── database
│   └── schema.sql
└── frontend
    ├── index.html
    ├── css
    │   └── style.css
    └── js
        └── app.js
```

The repository also contains generated files under `backend/build/`. They are
build artifacts rather than source files and are omitted from the tree above.

## Setup and running

### Frontend

Open `frontend/index.html` in a browser or serve the `frontend` directory with
a local static HTTP server. The page expects the backend at
`http://localhost:3000`.

### Database

Create a PostgreSQL database named `parking_management`, then run:

```text
psql -U <user> -d parking_management -f database/schema.sql
```

The schema file creates the `pgcrypto` extension and all 14 tables. Do not put
real passwords in documentation or source code.

For an existing database created before Phase 2 authentication, run the
repeatable migration:

```text
psql -U <user> -d parking_management -f database/phase2_auth.sql
```

Apply the Phase 3 parking-assignment migration after the schema, any Phase 2
migration, and any optional seed data, but before starting the updated backend:

```text
psql -U <user> -d parking_management -f database/phase3_vehicle_parking.sql
```

This migration creates the active parking-assignment table, clears seeded
`occupied` and `reserved` slot statuses when no active reservation/session/
assignment owns them, and deletes vehicle records that are not referenced by
reservation, session, or parking-assignment history. Linked vehicles and
historical records are preserved. Maintenance and unavailable slots are left
unchanged.

For local Phase 1 development data, run the repeatable seed script after the
schema:

```text
psql -U <user> -d parking_management -f database/seed_phase1.sql
```

The seed creates three Mall Central floor areas with 32 slots each. It includes
four truck slots, ten bike/scooter slots, twelve normal car slots, three EV
slots, and three handicapped slots per floor. It uses example availability
states and must not be used as production data.

### Backend

The backend requires CMake 3.20 or newer, a C++20 compiler, Crow, and
libpqxx. Configure and build from the `backend` directory using the existing
CMake configuration:

```text
cmake -S backend -B backend/build
cmake --build backend/build
```

Before starting the backend, configure the database password through the
environment. The other variables have local-development defaults:

```text
PARKING_DB_HOST=localhost
PARKING_DB_PORT=5432
PARKING_DB_NAME=parking_management
PARKING_DB_USER=postgres
PARKING_DB_PASSWORD=<your-local-password>
PARKING_SERVER_PORT=3000
```


On Windows PowerShell, for the current terminal session:

```powershell
$env:PARKING_DB_PASSWORD = "<your-local-password>"
```

Run the generated `parking_backend` executable. It listens on the configured
server port. The backend exits with a configuration error if
`PARKING_DB_PASSWORD` is missing or `PARKING_SERVER_PORT` is invalid.

## Documentation

- [planning.md](planning.md) — requirements, progress, and next steps
- [database.md](database.md) — exact PostgreSQL schema documentation
- [API.md](API.md) — implemented API routes and pending API work
- [OOPS.md](OOPS.md) — C++ object-oriented programming analysis

## Limitations and next steps

The project currently demonstrates the initial frontend/backend/database
connection and Phase 2 authentication/profile flows. The next implementation
step is to replace the remaining test routes with validated application
services and REST resources, then connect those services to the existing
schema. Input validation, secrets management, and production deployment remain
necessary before the system can be considered complete.
