# AI Prompt Documentation

This file documents the prompts used to build the Vehicle Parking Management
System with AI assistance. The prompts were documented retroactively and
reconstructed from the project's code, commit history, and planning documents.

**Stack:** HTML/CSS/JavaScript frontend, C++20 Crow backend, PostgreSQL with
libpqxx, CMake.

---

## Phase 1: Parking availability (Version 1)

Commit reference: `cb4c52c feat: complete phase 1 parking availability`
(merged in PR #1).

### Prompt 1: Project setup and planning

> I'm building a college project called "Vehicle Parking Management System". The stack is an HTML/CSS/JavaScript frontend, a C++20 backend using the Crow framework, and a PostgreSQL database accessed through libpqxx. Build tooling is CMake. Create the folder structure (`frontend/`, `backend/src/`, `database/`) and a README with the project overview, architecture diagram (frontend -> HTTP/JSON -> Crow backend -> libpqxx -> PostgreSQL), and a phase-wise plan. Phase 1 should be "parking availability": showing parking areas and slot status from the database.

**Result:** folder structure, `README.md`, `planning.md`.

### Prompt 2: Database schema and seed data

> Design a PostgreSQL schema for the parking system with the core tables (users, vehicles, parking areas/floors, parking slots, reservations, parking sessions, payments, rates, etc.), with primary and foreign keys, constraints, and indexes. Slots must have a status of available, occupied, reserved or maintenance, and a vehicle type. Write `schema.sql` and a `seed_phase1.sql` that inserts 3 floors with a realistic mix of slots in different states (including occupied and maintenance ones), so the availability page has real data to show. Also document the tables in `database.md`.

**Result:** `database/schema.sql`, `database/seed_phase1.sql`, `database.md`.

### Prompt 3: C++ Crow backend with a database connection

> Create a C++20 Crow backend in `backend/src/` with CMake (Crow fetched via FetchContent, libpqxx for PostgreSQL). Use a centralized config module for DB credentials and port, and a database helper class that connects to PostgreSQL. Add CORS support so the frontend can call it from the browser. Add two test routes first, `/api/hello` and `/api/database`, to confirm the server and DB connectivity work. Keep the code modular (`config/`, `database/`, `main.cpp`).

**Result:** `backend/CMakeLists.txt`, `backend/src/config/`, `backend/src/database/`, `backend/src/main.cpp`.

### Prompt 4: Parking availability REST endpoints

> Extend the backend with read-only endpoints for Phase 1: one returning all parking areas/floors with their slot counts, and one returning the slots for a given area, including slot number, vehicle type and status. Return JSON with a consistent response shape and error format. Handle an invalid area ID, an empty area and database failures with proper HTTP status codes. Put the response builders and error helpers in `api/` headers, and write unit tests for the response shapes. Document the endpoints in `API.md`.

**Result:** `GET /api/parking-areas`, `GET /api/parking-areas/{area_id}/slots`, `backend/src/api/`, `backend/tests/api_tests.cpp`, `API.md`.

### Prompt 5: Frontend parking availability page

> Build the frontend (`index.html`, `css/style.css`, `js/app.js`) that fetches areas and slots from the backend API using `fetch`. Show a floor selector and a slot grid where each slot's state (available, occupied, reserved, maintenance) is shown by colour, with a legend and summary counts. Add top-view vehicle SVG icons for car, bike, scooter, EV and truck, plus a parking background image. Handle loading, empty and error states with a retry button. Make the layout clean and responsive.

**Result:** `frontend/index.html`, `frontend/css/style.css`, `frontend/js/app.js`, `frontend/assets/images/`.

---

## Phase 2: Authentication and profiles

Commit reference: `2fd5926 feat: complete phase 2 authentication`
(merged in PR #2).

### Prompt 1: Planning and database changes

> Phase 1 (parking availability) is done. For Phase 2 I want authentication and user profiles. Review my existing PostgreSQL schema and write a migration `database/phase2_auth.sql` for databases that already exist. It should add a unique `username` column to `users` (backfilling existing rows), remove the unused `google_subject` column, and create an `auth_sessions` table with `session_id`, `user_id` (FK to users, ON DELETE CASCADE), a unique `token_hash`, `expires_at` and `created_at`, plus an index on `token_hash`. Wrap it all in a transaction and make it safe to re-run. Update `schema.sql` and `database.md` to match.

**Result:** `database/phase2_auth.sql`, updated `schema.sql` and `database.md`.

### Prompt 2: Password hashing and session token module

> In my C++ Crow backend, create a modular `backend/src/auth/` (`auth.h` and `auth.cpp`) using OpenSSL. It needs `hashPassword` (salted and slow-hashed) and `verifyPassword`, and `createToken`, which generates a secure random session token. It also needs `hashToken`, because only the hash of the token is stored in the database and the raw token is never stored. Don't store plain-text passwords anywhere. Update CMake to link OpenSSL, and write `auth_tests.cpp` covering correct and wrong passwords, unique tokens, and hash consistency.

**Result:** `backend/src/auth/auth.h`, `backend/src/auth/auth.cpp`, `backend/tests/auth_tests.cpp`.

### Prompt 3: Registration, login, logout and session endpoints

> Add these auth endpoints to the Crow backend: `POST /api/auth/register`, `POST /api/auth/login`, `POST /api/auth/logout`, and `GET /api/auth/me`. Registration should validate the username, email and password, and reject duplicate emails and usernames. Login should verify the password, reject invalid credentials and inactive accounts with a generic message, and create a session row with an expiry. Authenticated routes read the `Authorization: Bearer <token>` header. Logout deletes the session. Use the existing error format from `api/errors.h`. Put the input checks in a separate `validation/` module with unit tests.

**Result:** auth routes in `backend/src/main.cpp`, `backend/src/validation/`, `backend/tests/validation_tests.cpp`.

### Prompt 4: Profile endpoints and route protection

> Add the protected profile endpoints: `GET /api/profile`, `PUT /api/profile` to update name, phone and similar fields, and `PUT /api/profile/password`, which requires the current password and validates the new one. Every route must return 401 for a missing, invalid or expired token. Update the CORS settings to allow the Authorization header and the PUT method. Update `API.md` with the request and response examples for all 7 endpoints. Add the matching cases to `api_tests.cpp`.

**Result:** profile routes, updated CORS settings, `API.md`, `backend/tests/api_tests.cpp`.

### Prompt 5: Frontend login, register and protected views

> Update the frontend (`index.html`, `style.css`, `app.js`) to add an auth gate. Show a login screen and a register screen, with switching links, a "Remember me" checkbox, a "Forgot password?" button, and an error message area. The parking availability page from Phase 1 stays hidden until the user is logged in. After login, store the token (localStorage if "remember me" is checked, otherwise sessionStorage). On page load, call `/api/auth/me` to restore the session. Show a top bar with an avatar button that logs the user out. Send the Bearer token on every API call and return to the login screen on a 401. Keep the style consistent with Phase 1.

**Result:** auth gate in `frontend/index.html`, `frontend/css/style.css`, `frontend/js/app.js`.

---

## Phase 3: Vehicle management (Version 1)

Commit reference: `1029993 feat: complete phase 3 vehicle parking` (merged in PR #3).

### Prompt 1: Database migration for vehicle assignments

> Implement Phase 3 vehicle management in PostgreSQL: create `parking_assignments` table with unique indexes on active assignments per vehicle and per slot, update stale slot statuses to available, and document the schema in `database/phase3_vehicle_parking.sql` and `database.md`.

**Result:** `database/phase3_vehicle_parking.sql`, updated `schema.sql` and `database.md`.

### Prompt 2: Vehicle CRUD endpoints

> Add owner-scoped vehicle endpoints in C++ Crow: `GET /api/vehicles`, `POST /api/vehicles`, `PUT /api/vehicles/{id}`, `DELETE /api/vehicles/{id}` with uppercase normalization, vehicle type checks (`car`, `bike`, `scooter`, `truck`, `bus`, `other`), EV support, and duplicate registration protection.

**Result:** `backend/src/main.cpp`, `backend/src/api/vehicle_responses.h`.

### Prompt 3: Compatible slot lookup, parking assignment, and checkout

> Add endpoints to find available compatible slots for a vehicle (`GET /api/vehicles/{id}/available-slots`), park the vehicle atomically (`POST /api/vehicles/{id}/park`), and check out (`POST /api/vehicles/{id}/checkout`).

**Result:** parking assignment endpoints in `backend/src/main.cpp`.

### Prompt 4: Frontend vehicle management and parking UI

> Extend `frontend/index.html`, `style.css`, and `app.js` with a "Your vehicles" panel allowing users to add, edit, and delete vehicles, view available slots matching their vehicle, park in a selected slot, view active parking status on the map, and check out.

**Result:** `frontend/index.html`, `frontend/css/style.css`, `frontend/js/app.js`.

---

## Phase 4: Basic administration and maintenance (Version 1)

### Prompt 1: Admin authorization & data response serializers

> Create `backend/src/api/admin_responses.h` with JSON serialization helpers for administrative entities (`adminUserJson`, `adminAreaJson`, `adminSlotJson`, `adminRateJson`, `adminMaintenanceJson`). Add `findAuthenticatedAdmin()`, `forbidden()`, and number/integer parser helpers in `main.cpp`. Write unit tests in `backend/tests/admin_tests.cpp`.

**Result:** `backend/src/api/admin_responses.h`, `backend/tests/admin_tests.cpp`, `backend/CMakeLists.txt`.

### Prompt 2: User and Area administration endpoints

> In `backend/src/main.cpp`, implement administrator-only endpoints: `GET /api/admin/users`, `PUT /api/admin/users/{id}/role` (user/admin), `PUT /api/admin/users/{id}/status` (activate/deactivate), preventing self-demotion or self-deactivation. Implement area CRUD: `GET /api/admin/parking-areas`, `POST /api/admin/parking-areas`, `PUT /api/admin/parking-areas/{id}`, and `DELETE /api/admin/parking-areas/{id}` with conflict checks (`409 AREA_IN_USE`).

**Result:** user and area admin routes in `backend/src/main.cpp`.

### Prompt 3: Slot, Rate, and Maintenance management endpoints

> Implement slot CRUD under areas (`GET /api/admin/parking-areas/{id}/slots`, `POST /api/admin/parking-areas/{id}/slots` with duplicate prevention, `PUT /api/admin/parking-slots/{id}`, `DELETE /api/admin/parking-slots/{id}` preventing deletion of occupied slots). Implement rate rules CRUD (`GET /api/admin/parking-rates`, `POST`, `PUT`, `DELETE` with `409 DUPLICATE_RATE` protection). Implement maintenance CRUD (`GET /api/admin/maintenance`, `POST`, `PUT`, `DELETE`) enriched with slot and area names.

**Result:** slot, rate, and maintenance admin routes in `backend/src/main.cpp`.

### Prompt 4: Admin frontend console and navigation

> Extend `frontend/index.html`, `frontend/css/style.css`, and `frontend/js/app.js` to detect admin role on login, display an ADMIN badge and View Switcher (`User View` / `Admin Console`), and provide tabbed management screens for Users, Parking Areas, Parking Slots, Parking Rates, and Slot Maintenance with real-time feedback, inline forms, role/status toggles, and data validation.

**Result:** admin console in `frontend/index.html`, `frontend/css/style.css`, `frontend/js/app.js`.

### Prompt 5: Development seed data and live end-to-end verification

> Create `database/seed_phase4_admin.sql` seeding a default administrator account (`admin@example.com` / `AdminPass123!`), baseline rate matrices, and active maintenance periods. Write an end-to-end test script verifying all 18 admin operations against live PostgreSQL and Crow backend. Update `API.md`, `planning.md`, and `README.md`.

**Result:** `database/seed_phase4_admin.sql`, `scratch/e2e_admin_test.py`, documentation updates.

---

## Notes

- Phase 2 status in `planning.md`: runtime verification and end-to-end
  error-path testing (duplicate emails, invalid credentials, inactive accounts,
  unauthenticated access) remain.

