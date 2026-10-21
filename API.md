# REST API

## Current status

The API includes authentication, profiles, owner-scoped vehicle management,
parking availability, and Phase 5 reservation operations.

| Component | Current status |
| --- | --- |
| Crow backend | Implemented with multi-threaded routing |
| REST endpoints | Health, parking availability, vehicles, reservations, and administrator endpoints implemented |
| PostgreSQL through C++ | libpqxx parameterized queries and transactions |
| Frontend API integration | Authentication, parking availability, vehicle and reservation management, and admin console |
| Authentication and authorization | Bearer sessions with role-based access (`user`, `admin`) |
| Vehicle management | Authenticated CRUD and parking assignments implemented |
| Reservations | Availability filters, transactional booking, owner-scoped listing/details, and cancellation |
| Administrator management | Users, areas, slots, rates, and maintenance operations implemented |

The database tables are not API resources until backend routes and operations
are implemented for them.

## Implemented endpoints

### `POST /api/auth/register`

Creates a user account and returns a seven-day bearer token. The JSON body
requires separate `username`, `email`, and password fields; the password must
be at least eight characters. `phone` is optional. Duplicate usernames or
emails return `409 ACCOUNT_IDENTIFIER_ALREADY_REGISTERED`.

### `POST /api/auth/login`

Accepts a username or email in the `email` field plus `password`, and returns
a bearer token and user profile.
Invalid credentials and inactive accounts return `401 INVALID_CREDENTIALS`.

### `POST /api/auth/logout`

Invalidates the bearer token supplied in the `Authorization: Bearer <token>`
header.

### `GET /api/auth/me`

Returns the authenticated user. Requests without a valid bearer token return
`401 UNAUTHENTICATED`.

### `GET /api/profile`

Returns the authenticated user's profile.

### `PUT /api/profile`

Updates the authenticated user's `name` and optional `phone`.

### `PUT /api/profile/password`

Updates the authenticated user's password. The JSON body requires
`current_password` and a `new_password` of at least eight characters.

### `GET /api/vehicles`

Returns only vehicles owned by the authenticated user.

```json
{
  "vehicles": [
    {
      "vehicle_id": 4,
      "registration_number": "ABC123",
      "vehicle_type": "car",
      "is_electric": true
    }
  ]
}
```

Requests without a valid bearer token return `401 UNAUTHENTICATED`. Database
failures return `503 DATABASE_UNAVAILABLE`. Parked vehicles include a `parking`
object containing the active slot and parking start time.

### `POST /api/vehicles`

Creates a vehicle for the authenticated user. Registration numbers are trimmed
and converted to uppercase. The JSON body requires `registration_number` (1-20
characters) and `vehicle_type` (`car`, `bike`, `scooter`, `truck`, `bus`, or
`other`). `is_electric` is an optional boolean that defaults to `false`.

Returns `201 Created` with the created vehicle in a `vehicle` object.
Duplicate registration numbers return `409
VEHICLE_REGISTRATION_ALREADY_EXISTS`; invalid fields return `400` with a
specific error code. The owner is taken from the authenticated session, never
from the request body.

After a successful create, the frontend shows available slots compatible with
the vehicle.

### `GET /api/vehicles/{vehicle_id}/available-slots`

Returns active-area slots that are currently available and match the vehicle's
type. EV charging slots are included only when the vehicle is marked electric.
Only the vehicle's owner can request its slots. A vehicle that is already parked
returns `409 VEHICLE_ALREADY_PARKED`.

### `POST /api/vehicles/{vehicle_id}/park`

Assigns the owner's vehicle to an available compatible slot. The JSON body
requires a positive `slot_id`. The operation atomically marks the slot
occupied and records the active assignment. EV charging slots require the
vehicle to be marked electric. A slot that is no longer available, is inactive,
or is incompatible returns `409 PARKING_SLOT_UNAVAILABLE`.

Slots with any pending, confirmed, or active reservation that has not ended are
not eligible for direct parking, so a future reservation cannot be displaced
by a manual assignment.

The response contains a `parking` object with the assignment, vehicle, slot,
area, and parking start time. An active slot remains occupied until checkout.
This direct assignment tracks occupancy only; it does not create a reservation
or a billable `parking_session`.

### `POST /api/vehicles/{vehicle_id}/checkout`

Checks out the owner's vehicle, records the checkout time, and releases its
slot back to `available`. A vehicle without an active assignment returns `404
ACTIVE_PARKING_NOT_FOUND`.

`GET /api/vehicles` includes the active assignment as `parking` when the
vehicle is parked; otherwise that property is omitted. It also includes
`can_delete`, which is true only when the vehicle is not actively parked.
Vehicle edits are rejected while the vehicle is parked.

### `PUT /api/vehicles/{vehicle_id}`

Updates the authenticated user's vehicle using the same fields as the create
request. Returns the updated vehicle in a `vehicle` object. A vehicle that
does not exist or belongs to another user returns `404 VEHICLE_NOT_FOUND`.
Duplicate registration numbers return `409
VEHICLE_REGISTRATION_ALREADY_EXISTS`.

### `DELETE /api/vehicles/{vehicle_id}`

Deletes the authenticated user's vehicle and returns `204 No Content`. A
vehicle that does not exist or belongs to another user returns `404
VEHICLE_NOT_FOUND`. An actively parked vehicle returns `409
VEHICLE_CURRENTLY_PARKED` and must be checked out first. After checkout,
deletion permanently removes the vehicle and its associated assignments,
reservations, sessions, payments, and receipts in one database transaction.

### `GET /api/reservations/availability`

Searches active parking areas for slots compatible with the authenticated
user's vehicle and requested time. Required query parameters are `vehicle_id`,
`start_time`, and `end_time`; timestamps must be future ISO-8601 values with a
timezone offset. Optional filters are `area_id`, `location` (matches area name
or location), `slot_type`, and `ev_only=true`. Results include slot and area
details and include `hourly_rate` when a matching rate is configured.

Slots must be active, available, vehicle-compatible, and free of active
parking or overlapping pending, confirmed, or active reservations. Time
intervals use half-open boundaries, so one reservation may start exactly when
another ends. Scheduled or in-progress maintenance is excluded for the
requested interval. Requests for another user's vehicle return no results.

### `POST /api/reservations`

Creates a confirmed reservation for the authenticated user. The JSON body
requires positive `vehicle_id` and `slot_id` values plus future ISO-8601
`start_time` and `end_time` values with timezone offsets. The operation checks
vehicle ownership, time order, vehicle/slot compatibility, area and slot
status, current parking, and overlapping reservations inside a transaction
while locking the selected slot.
The operation also rejects slots with scheduled or in-progress maintenance
during any part of the requested interval.

Returns `201 Created` with a `reservation` object containing its ID, vehicle,
slot, area, time range, status, and configured hourly rate when available.
Invalid timestamps return `400 INVALID_TIME_RANGE`; an unknown or unowned
vehicle returns `404 VEHICLE_NOT_FOUND`; a vehicle or slot conflict returns
`409 VEHICLE_UNAVAILABLE` or `409 PARKING_SLOT_UNAVAILABLE`.

### `GET /api/reservations`

Returns the authenticated user's reservations, newest start time first. Other
users' reservations are never included.

### `GET /api/reservations/{reservation_id}`

Returns the requested reservation only when it belongs to the authenticated
user. Unknown and unowned reservation IDs both return `404
RESERVATION_NOT_FOUND`.

### `DELETE /api/reservations/{reservation_id}`

Cancels an owned reservation with status `pending` or `confirmed` and returns
its ID with status `cancelled`. Active, completed, expired, or already
cancelled reservations return `409 RESERVATION_NOT_CANCELLABLE`.

### `GET /api/hello`

Returns a JSON message confirming that the C++ backend route is running.

Example response:

```json
{
  "message": "C++ backend is working"
}
```

### `GET /api/database`

Returns the database connection result recorded when the server started. The
backend attempts to connect to PostgreSQL and executes `SELECT 1` before
starting the server.

Successful response:

```json
{
  "status": "success",
  "message": "C++ backend connected to PostgreSQL",
  "database": "parking_management"
}
```

When the startup database check failed, the endpoint returns HTTP `503
Service Unavailable` with the standard API error shape:

```json
{
  "error": {
    "code": "DATABASE_UNAVAILABLE",
    "message": "PostgreSQL is unavailable. Check the backend logs."
  }
}
```

The route reports the startup connectivity result; it does not perform a new
database query for each request. Other future API errors should use the same
`error.code` and `error.message` structure.

### `GET /api/parking-areas`

Returns every configured parking area. Under the Phase 1 design, each mall
floor is represented as a separate parking area. `available_slots` is
calculated from the current slot statuses.

Example response:

```json
{
  "areas": [
    {
      "area_id": 1,
      "name": "Mall Central — Ground Floor",
      "location": "Mall Central",
      "operating_hours": "07:00 - 22:00",
      "capacity": 32,
      "status": "active",
      "available_slots": 12
    }
  ]
}
```

Database failures return `503 DATABASE_UNAVAILABLE`.

### `GET /api/parking-areas/{area_id}/slots`

Returns the selected floor and all of its configured parking slots.
Slots with a live vehicle assignment or active parking session include
`vehicle_registration`, `parked_at` (UTC ISO-8601), and `parked_seconds`.
Legacy `occupied` or `reserved` slot statuses without a current assignment,
session, or reservation are reported as available so seeded stale states do not
look like cars are still parked.

Example response:

```json
{
  "area": {
    "area_id": 1,
    "name": "Mall Central — Ground Floor"
  },
  "slots": [
    {
      "slot_id": 1,
      "area_id": 1,
      "slot_number": "G-A01",
      "vehicle_type": "car",
      "slot_type": "standard",
      "is_ev": false,
      "status": "available"
    }
  ]
}
```

An unknown area returns `404 PARKING_AREA_NOT_FOUND`. Database failures
return `503 DATABASE_UNAVAILABLE`.

### `GET /api/admin/users`

Returns all registered users with their account ID, username, name, email,
phone, role (`user` or `admin`), `is_active` status, and UTC creation timestamp.
Requires an authenticated bearer session with the `admin` role. Non-administrators
receive `403 FORBIDDEN`.

### `PUT /api/admin/users/{user_id}/role`

Updates a user's role to `'user'` or `'admin'`. Administrators are prevented
from changing their own role (`400 CANNOT_MODIFY_OWN_ADMIN_ACCESS`).

### `PUT /api/admin/users/{user_id}/status`

Updates a user's `is_active` status. Inactive accounts cannot log in. Administrators
are prevented from deactivating their own account (`400 CANNOT_MODIFY_OWN_ADMIN_ACCESS`).

### `GET /api/admin/parking-areas`

Returns all parking areas with their current `slot_count`.

### `POST /api/admin/parking-areas`

Creates a new parking area. Requires `name` (1-150 chars), `location`, non-negative
`capacity`, and `status` (`active`, `inactive`, or `closed`). `operating_hours`,
`latitude`, and `longitude` are optional. Returns `201 Created`.

### `PUT /api/admin/parking-areas/{area_id}`

Updates an existing parking area's details and configuration.

### `DELETE /api/admin/parking-areas/{area_id}`

Deletes a parking area. If the area still contains slots or rate rules, returns
`409 AREA_IN_USE`.

### `GET /api/admin/parking-areas/{area_id}/slots`

Returns all parking slots belonging to the specified area, including slot number,
allowed vehicle type, slot type, EV flag, and status.

### `POST /api/admin/parking-areas/{area_id}/slots`

Adds a slot to the area. Requires `slot_number`, `vehicle_type` (`car`, `bike`,
`scooter`, `truck`, `bus`, `other`), `slot_type`, `is_ev`, and `status`. Returns
`409 DUPLICATE_SLOT_NUMBER` if a slot with the same number exists in the area.

### `PUT /api/admin/parking-slots/{slot_id}`

Updates slot properties or marks a slot as `available`, `reserved`, `occupied`,
`maintenance`, or `unavailable`.

### `DELETE /api/admin/parking-slots/{slot_id}`

Deletes a parking slot. Returns `409 SLOT_IN_USE` if the slot currently has an
active parking assignment or in-progress session.

### `GET /api/admin/parking-rates`

Lists all pricing rules across all areas, detailing `hourly_rate`, `overstay_rate`,
and `ev_charging_rate`.

### `POST /api/admin/parking-rates`

Defines a rate rule for an area, vehicle type, and slot type. Returns `409
DUPLICATE_RATE` if a rate for the same combination already exists.

### `PUT /api/admin/parking-rates/{rate_id}`

Updates hourly, overstay, or EV charging rates for a rate rule.

### `DELETE /api/admin/parking-rates/{rate_id}`

Deletes a rate rule.

### `GET /api/admin/maintenance`

Lists all maintenance records with enriched `slot_number` and `area_name` values,
sorted newest first.

### `POST /api/admin/maintenance`

Schedules a maintenance window for a slot. Requires `slot_id`, `reason`,
`start_time` (ISO-8601), optional `end_time` (ISO-8601), and `status` (`scheduled`,
`in_progress`, `completed`, or `cancelled`).

### `PUT /api/admin/maintenance/{maintenance_id}`

Updates an existing maintenance record.

### `DELETE /api/admin/maintenance/{maintenance_id}`

Removes a maintenance record.

## Frontend usage

`frontend/js/app.js` calls the documented routes using `fetch` against
`http://localhost:3000`. Vehicle requests include the authenticated bearer
token.

## Planned API work

The following areas are planned, not implemented:

- Parking-area, slot, and rate management writes
- Availability and reservation operations
- QR-token check-in
- Parking sessions and checkout
- Payments, receipts, and notifications

Request and response contracts should be added here only when their routes are
implemented and tested.
