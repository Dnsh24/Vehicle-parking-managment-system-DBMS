# Vehicle Parking Management System Plan

## 1. Project status

### Completed

- Basic HTML/CSS/JavaScript frontend test page
- Crow C++ server foundation
- Environment-based backend configuration
- PostgreSQL connection check using libpqxx
- Reusable backend validation utilities with focused tests
- Parameterized database read and write operations with transaction handling
- Database foundation failure test
- Configuration and API error-response tests
- Foundation runtime verification, health checks, and CORS verification
- `GET /api/hello`
- `GET /api/database`
- Phase 1 parking availability: database-backed areas and slots load in the
  frontend
- Phase 2 authentication: registration, login, logout, sessions, and profiles
- Phase 3 vehicle management: authenticated vehicle CRUD, EV-compatible slot
  selection, parking, checkout, map display, and live end-to-end verification
- Phase 5 search and reservations: backend, frontend, unit tests, and
  user-verified end-to-end flow completed
- Phase 8 history and in-app notifications: user-scoped paginated history and
  notifications, read/unread controls, and transactional workflow event
  notifications implemented; live database end-to-end verification remains
- PostgreSQL schema with 12 core tables, constraints, and indexes

### In progress

- Application-specific backend and integration testing

### Planned

- Core parking operations: live database end-to-end verification and testing

The existence of a database table does not mean that its related application
feature is implemented.

## 2. Project objective

Build a centralized parking-management platform for users and administrators.
The intended system connects users, vehicles, parking areas, parking slots,
reservations, entry and exit operations, payments, notifications, and reports.

The technology stack is:

- Frontend: HTML, CSS, JavaScript
- Backend: C++ with Crow
- Database: PostgreSQL
- Communication: REST APIs

## 3. Functional requirements

### User accounts

Users should be able to register, log in, log out, manage profiles, change
passwords, and view their registered vehicles.

**Status:** Backend and frontend authentication/profile flows implemented;
runtime verification and end-to-end error-path testing remain.

### Vehicles

Users should be able to add, edit, delete, and view vehicles. Vehicle types
and EV status must be validated. A vehicle must belong to the authenticated
user.

**Status:** Completed. Authenticated vehicle CRUD, owner validation, EV-aware
slot selection, parking and checkout, and map occupant details are implemented
and have been verified end to end.

### Parking areas and slots

Administrators should manage multiple parking areas, capacities, locations,
operating hours, parking slots, vehicle types, slot types, and EV capability.

Slot states are:

- `available`
- `reserved`
- `occupied`
- `maintenance`
- `unavailable`

**Status:** Planned. The schema supports these values, but management APIs and
screens do not exist.

### Availability and search

Users should search by area, vehicle type, slot type, EV capability,
availability, reservation time, and location.

A slot is selectable only when it:

- Supports the vehicle type
- Meets EV requirements
- Is not unavailable or under maintenance
- Has no conflicting reservation

Real-time availability must not be claimed until backend state updates and
frontend refresh behavior are implemented.

**Status:** Planned.

### Reservations

Users should create, view, and cancel reservations with:

- User
- Vehicle
- Area
- Slot
- Start time
- End time
- Status
- Reservation ID

Reservation statuses are:

- `pending`
- `confirmed`
- `active`
- `completed`
- `expired`
- `cancelled`

Reservation creation must validate ownership, time range, compatibility,
availability, and maintenance state. It must use a transaction to prevent
conflicting bookings.

**Status:** Completed. Availability search, owner-scoped reservation
creation/list/details/cancellation, transaction-protected conflict checks, and
the end-to-end user flow are working. The user verified the running application.

### Manual check-in and vehicle entry

Manual check-in allows an authorized user to activate their confirmed reservation:

```text
Reservation → Manual check-in → Validate user, vehicle, time, and slot
→ Create parking session → Mark reservation active and slot occupied
```

**Status:** Implemented in Phase 7. Manual check-in is supported through reservation routes.

### Parking sessions and exit

On entry, create a parking session and record `entry_time`. On exit:

1. Identify the active session.
2. Record `exit_time`.
3. Calculate duration.
4. Calculate overstay and fees.
5. Complete the session and reservation.
6. Release the slot only when it is not under maintenance.
7. Continue to payment and receipt generation.

The entry and exit state changes must be transactional.

**Status:** Planned. The `parking_sessions` table exists only.

### Overstay and fees

Overstay is calculated from the reserved end time and actual exit time:

```text
overstay_duration = max(0, actual_exit_time - reserved_end_time)
```

Fees may depend on parking duration, vehicle type, area, slot type, EV usage,
reservation duration, and overstay duration.

```text
base_fee = parking_duration × hourly_rate
total_fee = base_fee + overstay_fee + additional_charges
```

Billing units, rounding, minimum charges, and missing-rate behavior must be
defined before implementation.

**Status:** Planned. Rate and fee tables exist, but no calculation service
exists.

### Payments and fee settlements

Users settle fees upon completing a parking session. Payment statuses are:

- `pending`
- `successful`
- `failed`
- `refunded`

**Status:** Implemented in Phase 7. Manual checkout settlement and status are recorded in the `payments` table.

### History and notifications

Users should view previous reservations, sessions, durations, payment status, and
amounts.

Notifications should support reservation confirmation, expiry, check-in,
overstay, payment, session completion, and maintenance events.

**Status:** Implemented in Phase 8. User-scoped in-app notifications,
reservation/session event notifications, unread tracking, and mark-as-read
controls are available. External notification delivery remains planned.

## 4. Administrator requirements

Administrators should be able to:

- View dashboard statistics
- Manage users and account access
- Manage parking areas and slots
- Configure EV slots and rates
- Schedule slot maintenance
- Monitor and manage reservations
- Monitor payments and refunds
- Generate parking, revenue, reservation, and overstay reports

Dashboard statistics should include users, vehicles, areas, slots, slot
statuses, reservations, payments, revenue, and overstays.

**Status:** Basic administrator management and maintenance completed in Phase 4.
Admin authentication, user role and status controls, area/slot management, rate
rules, and maintenance scheduling endpoints and frontend console are implemented
and verified. 

## 5. Build phases

Each phase follows this sequence:

```text
Database review → Backend logic and API → Backend tests
→ Frontend screen → Frontend integration → End-to-end tests
→ Update API.md and this plan
```

Proposed endpoints must not be added to [API.md](API.md) until implemented
and tested.

### Core parking operations

The project delivers the complete user journey:

```text
Register → Add vehicle → Find a slot → Reserve
→ Check in manually → Park → Check out manually
→ Calculate fee → View history and notifications
```

The core operations include basic overstay billing and in-app notifications.

#### Phase 0: Foundation

**Status:** Completed.

- Centralize configuration and database access.
- Add validation utilities and consistent JSON errors.
- Add parameterized-query and transaction conventions.
- Add backend tests.
- Keep the health endpoints working.
- Verify runtime startup, PostgreSQL connectivity, CORS behavior, and secret
  safety.

#### Phase 1: Parking availability

**Status:** Completed. Backend endpoints, frontend API integration, and
database-backed runtime verification are working with seeded parking slots.

- Backend: query areas and slots and expose read-only REST endpoints.
- Frontend: load areas and slots from the API, select a floor, and show slot
  states with loading, empty, error, and retry states.
- Backend response-shape tests are implemented, and the seeded three-floor
  PostgreSQL data loads successfully in the frontend.
- Test invalid areas, empty areas, database failures, occupied slots, and
  maintenance slots against the running database.

References: `parking_areas`, `parking_slots`, and `maintenance` in
[database.md](database.md).

#### Phase 2: Authentication and profiles

**Status:** Backend and frontend flows implemented; runtime verification and
end-to-end error-path testing remain.

- Backend: registration, login, logout, password hashing, authorization, and
  profile operations.
- Frontend: registration, login, logout, profile, and protected views.
- Test duplicate emails, invalid credentials, inactive accounts, and
  unauthenticated access.

Reference: `users` in [database.md](database.md).

#### Phase 3: Vehicle management

**Status:** Completed. Backend, frontend, database migration, and live
end-to-end vehicle parking and checkout flows have been verified.

- Backend: authenticated vehicle CRUD and ownership validation.
- Backend: compatible-slot lookup, atomic vehicle parking, and checkout that
  releases the slot. These direct assignments track occupancy only; the
  reservation/session billing workflow remains in later phases.
- Frontend: vehicle list, add, edit, delete, vehicle type and EV controls,
  available-slot selection, and active parking/checkout display.
- Test duplicate registrations, invalid types, and unauthorized access.

Reference: `vehicles` in [database.md](database.md).

#### Phase 4: Basic administration and maintenance

**Status:** Completed. Backend administrator endpoints, response helpers, role
enforcement, frontend management console tabs, seed data, and live end-to-end
tests have been verified.

- Backend: administrator-only user management (list, role change, status toggle),
  parking area CRUD, slot CRUD with uniqueness and occupancy checks, rate
  matrix CRUD, and slot maintenance scheduling with enriched area information.
- Frontend: administrator console with topbar role detection, user/admin view
  switcher, and dedicated management screens for users, areas, slots, rates,
  and maintenance.
- Database: `database/seed_phase4_admin.sql` seeds default admin credentials
  (`admin@example.com` / `AdminPass123!`), baseline rate rules, and active
  maintenance periods.
- Testing: unit response tests in `backend/tests/admin_tests.cpp`, role checks,
  duplicate prevention, and live end-to-end integration verified.

References: Section 4; related tables in [database.md](database.md).

#### Phase 5: Search and reservations

**Status:** Completed. Backend build and unit tests pass, and the user verified
the end-to-end flow against the running application.

- Backend: filtered availability, reservation creation, listing, details,
  cancellation, and transaction-protected conflict checks.
- Frontend: filters, reservation form, confirmation, status, and cancellation.
- Validate ownership, compatibility, future ISO-8601 time ranges, overlapping
  reservations, and concurrent requests. Reservation creation locks the
  selected slot within a transaction to prevent concurrent conflicting claims.

References: `reservations`, `parking_slots`, and `parking_rates` in
[database.md](database.md).

#### Phase 6: Manual entry and parking sessions

**Status:** Implemented. Reservation check-in creates an active session
transactionally, and active sessions are shown in the user dashboard.

- Backend: validate the reservation check-in window, create sessions, record
  entry, and perform manual check-in state transitions.
- Frontend: reservation check-in button, check-in result, and active session
  screen.
- Test expired and unauthorized reservations, duplicate entry, and
  maintenance slots.

References: `reservations` and `parking_sessions` in
[database.md](database.md).

#### Phase 7: Manual exit, basic overstay billing, and fee summary

**Status:** Completed. Transactional manual exit, real-time fee preview, overstay
calculation, payment status recording, unit tests, and live end-to-end flows are
implemented and verified.

- Backend: transactional exit, duration, basic overstay calculation, rate rule
  resolution, and fee summary generation (`POST /api/parking-sessions/{id}/exit`,
  `GET /api/parking-sessions/{id}/fee-summary`).
- Frontend: active session fee summary modal, real-time fee breakdown with
  overstay alerts, payment settlement option (`paid_manually`, `unpaid`, `waived`),
  and automated slot liberation on the live map.
- Implemented fee calculation rule:
  ```text
  overstay_duration = max(0, actual_exit_time - reserved_end_time)
  total_fee = parking_duration × hourly_rate + overstay_duration × overstay_rate
  ```
- Recorded payment in `payments` table and updated slot and reservation states
  atomically within a single transaction.
- Verified normal exit, overstay calculations, duplicate exit rejection (409),
  ownership isolation (404), unauthenticated rejection (401), and slot liberation.

References: `parking_sessions` and `parking_rates` in [database.md](database.md).

#### Phase 8: Basic history and in-app notifications

**Status:** Backend and frontend implemented; backend build and existing unit
suite pass. Phase 8-specific and live database end-to-end verification remain.

- Backend: user-scoped history, notifications, unread status, and event
  notifications for reservation confirmation, cancellation, check-in, exit,
  overstay, and fee status.
- Frontend: history list, fee summaries, notification list, unread count, and
  individual and bulk mark-as-read behavior.
- History and notification endpoints enforce user scope and support bounded
  pagination. Notification records are written in the same transactions as
  reservation, check-in, cancellation, and exit state changes.
- Apply `database/phase8_history_notifications.sql` to add notification-list
  indexes.
- Verify user isolation, pagination, empty results, notification ownership,
  and event delivery against a running PostgreSQL/Crow application.

References: `reservations`, `parking_sessions`, and `notifications` in
[database.md](database.md).

## 6. Phase dependencies

```text
Foundation
  ↓
Parking availability
  ↓
Authentication
  ↓
Vehicles
  ↓
Basic administration and maintenance
  ↓
Search and reservations
  ↓
Manual entry and sessions
  ↓
Exit, basic overstay billing, and fee summary
  ↓
History and in-app notifications
```

Mark a feature **Completed** only after its backend, frontend, integration,
and relevant error-path tests pass.

## 7. Implemented scope

- User registration, login, logout, and basic profiles
- Vehicle management
- Parking-area and slot availability
- Basic administrator management
- Reservations and cancellation
- Manual check-in and check-out
- Parking sessions
- Basic duration and overstay billing
- Manual payment or fee-status recording
- Reservation and parking history
- In-app notifications and unread status

## 8. Documentation references

- [README.md](README.md) — overview, setup, and current implementation
- [API.md](API.md) — verified REST endpoints only
- [database.md](database.md) — actual PostgreSQL schema
## Final Version
This is the final version of the Vehicle Parking Management System project.

## Limitations and next steps