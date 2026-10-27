# Vehicle Parking Management System

A college project for managing vehicle parking through an HTML/CSS/JavaScript
frontend, a C++ backend using Crow, and a PostgreSQL database.

## Current development status

| Component | Status |
| --- | --- |
| Frontend | Authentication, parking availability, vehicles, reservations, active sessions, history, notifications, and administrator management implemented. |
| PostgreSQL | Schema implemented with 12 core tables, constraints, and indexes; Phase 8 notification indexes are in `database/phase8_history_notifications.sql`. See [database.md](database.md). |
| C++ backend | Crow server with PostgreSQL access, bearer authentication, transactional reservations and sessions, manual exit billing, history, notifications, and administration. |
| REST API | Health, authentication, profile, parking availability, vehicles, reservations, sessions, history, notifications, and administrator endpoints implemented. |
| Frontend-backend integration | Browser `fetch` integration targets `http://localhost:3000`; live Phase 8 database end-to-end verification remains. |

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

Authentication, profiles, parking availability, vehicle management, basic
administrator management and maintenance, reservations, manual check-in and
exit, fee summaries, history, and in-app notifications are implemented.

## Implemented features

### Frontend

- Supports registration, login, and logout.
- Loads parking-area and slot availability.
- Adds and edits vehicles, shows compatible available slots, and parks a
  selected vehicle until checkout.
- Searches compatible slots by vehicle, area, location, slot type, EV charging,
  and requested time; creates, views, and cancels reservations.
- Shows active parking sessions, parking history with fee/payment summaries,
  paginated in-app notifications, unread count, and mark-as-read controls.
- Administrator console with user role/status controls, area management, slot
  configuration, pricing rates, and maintenance scheduling.
- Uses bearer-authenticated requests to the backend at port 3000.

### Backend

- Creates a Crow application with CORS middleware.
- Attempts a PostgreSQL connection to the `vehicle_parking` database.
- Executes `SELECT 1` when the connection is established.
- Provides bearer-session authentication and profile operations.
- Loads parking areas and slots from PostgreSQL.
- Provides owner-scoped vehicle CRUD, compatible-slot selection, and checkout.
- Provides owner-scoped availability search and transaction-protected reservation
  creation, listing, details, and cancellation.
- Provides manual reservation check-in, session exit and fee calculation,
  user-scoped paginated history, and transactional in-app event notifications.
- Atomically claims/releases parking slots and persists assignment history.
- Provides administrator operations for users, areas, slots, rates, and maintenance
  with role-based access control (`admin` role enforcement).

### Database

- Defines the 12 core tables documented in [database.md](database.md).
- Defines primary keys, foreign keys, checks, unique constraints, and indexes.

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

Create a PostgreSQL database named `vehicle_parking`, then run:

```text
psql -U <user> -d vehicle_parking -f database/schema.sql
```

The schema file creates the `pgcrypto` extension and all 12 tables. Do not put
real passwords in documentation or source code.

For an existing database created before Phase 2 authentication, run the
repeatable migration:

```text
psql -U <user> -d vehicle_parking -f database/phase2_auth.sql
```

Apply the Phase 3 parking-assignment migration after the schema, any Phase 2
migration, and any optional seed data, but before starting the updated backend:

```text
psql -U <user> -d vehicle_parking -f database/phase3_vehicle_parking.sql
```

This migration creates the active parking-assignment table, clears seeded
`occupied` and `reserved` slot statuses when no active reservation/session/
assignment owns them, and deletes vehicle records that are not referenced by
reservation, session, or parking-assignment history. Linked vehicles and
historical records are preserved. Maintenance and unavailable slots are left
unchanged.

For an existing database, add the vehicle accessibility preference before
starting the updated backend:

```text
psql -U <user> -d vehicle_parking -f database/phase6_accessible_parking.sql
```

This migration defaults existing vehicles to not requiring accessible parking.
Vehicles marked as requiring accessible parking may use handicapped slots;
other vehicles are not offered those slots.

For local Phase 1 development data, run the repeatable seed script after the
schema:

```text
psql -U <user> -d vehicle_parking -f database/seed_phase1.sql
psql -U <user> -d vehicle_parking -f database/seed_phase4_admin.sql
psql -U <user> -d vehicle_parking -f database/seed_sample_data.sql
```

The seed scripts create:
- **`seed_phase1.sql`**: Three Mall Central floor areas with 32 slots each (96 slots total).
- **`seed_phase4_admin.sql`**: Administrator account (`dnshneela@gmail.com`), baseline rates, and maintenance.
- **`seed_sample_data.sql`**: Comprehensive evaluation sample data (at least 10-12 records in every major table: `users`, `vehicles`, `parking_areas`, `parking_rates`, `reservations`, `parking_sessions`, `payments`, `notifications`, `maintenance`).

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
PARKING_DB_NAME=vehicle_parking
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

## Final Version
This is the final version of the Vehicle Parking Management System project.

## Limitations and next steps

The project has implemented the core parking workflow through Phase 8.
Live database end-to-end verification for Phase 8 and production deployment
remain outstanding. Input validation, secrets management, and operational
hardening remain necessary before the system can be considered complete.
