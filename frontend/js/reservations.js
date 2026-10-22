// =====================================================================
// ParkFlow - Reservation Search & Management
// =====================================================================

const reservationSearchForm = document.getElementById("reservationSearchForm");
const reservationVehicle = document.getElementById("reservationVehicle");
const reservationArea = document.getElementById("reservationArea");
const reservationResults = document.getElementById("reservationResults");
const reservationList = document.getElementById("reservationList");
const reservationMessage = document.getElementById("reservationMessage");
const refreshReservationsButton = document.getElementById("refreshReservationsButton");
const activeParkingSessions = document.getElementById("activeParkingSessions");

function setReservationMessage(message, isError = false) {
    if (!reservationMessage) return;
    reservationMessage.textContent = message;
    reservationMessage.classList.toggle("is-error", isError);
}

function setReservationDateDefaults() {
    if (!reservationSearchForm) return;
    const startInput = reservationSearchForm.elements.start_time;
    const endInput = reservationSearchForm.elements.end_time;
    const start = new Date();
    start.setHours(start.getHours() + 1, 0, 0, 0);
    const end = new Date(start.getTime() + 60 * 60 * 1000);
    startInput.value = toLocalDatetimeInputValue(start.toISOString());
    endInput.value = toLocalDatetimeInputValue(end.toISOString());
}

function setSelectOptions(select, items, placeholder, getValue, getLabel) {
    if (!select) return;
    const previous = select.value;
    select.replaceChildren();
    const emptyOption = document.createElement("option");
    emptyOption.value = "";
    emptyOption.textContent = placeholder;
    select.append(emptyOption);
    items.forEach((item) => {
        const option = document.createElement("option");
        option.value = String(getValue(item));
        option.textContent = getLabel(item);
        select.append(option);
    });
    if (items.some((item) => String(getValue(item)) === previous)) {
        select.value = previous;
    }
}

function setReservationVehicleOptions(vehicles) {
    setSelectOptions(
        reservationVehicle,
        vehicles,
        "Choose a vehicle",
        (vehicle) => vehicle.vehicle_id,
        (vehicle) => `${vehicle.registration_number} · ${vehicle.vehicle_type}${vehicle.is_electric ? " · EV" : ""}`
    );
}

async function updateReservationVehicleOptions(vehicles) {
    if (!reservationVehicle) return;
    if (!vehicles) {
        const response = await fetch(`${API_BASE}/api/vehicles`, { headers: authHeaders() });
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.vehicles)) {
            throw new Error(data.error?.message || "Vehicles could not be loaded for reservations.");
        }
        vehicles = data.vehicles;
    }
    setReservationVehicleOptions(vehicles);
}

async function loadReservationOptions() {
    try {
        const [areaResponse, vehicleResponse] = await Promise.all([
            fetch(`${API_BASE}/api/parking-areas`, { headers: authHeaders() }),
            fetch(`${API_BASE}/api/vehicles`, { headers: authHeaders() })
        ]);
        const [areaData, vehicleData] = await Promise.all([
            areaResponse.json(),
            vehicleResponse.json()
        ]);
        if (!areaResponse.ok || !Array.isArray(areaData.areas)) {
            throw new Error(areaData.error?.message || "Parking areas could not be loaded.");
        }
        if (!vehicleResponse.ok || !Array.isArray(vehicleData.vehicles)) {
            throw new Error(vehicleData.error?.message || "Vehicles could not be loaded for reservations.");
        }
        setSelectOptions(
            reservationArea,
            areaData.areas.filter((area) => area.status === "active"),
            "All active areas",
            (area) => area.area_id,
            (area) => `${area.name} · ${area.location}`
        );
        setReservationVehicleOptions(vehicleData.vehicles);
    } catch (error) {
        setReservationMessage(error.message, true);
    }
}

function reservationSearchTimes() {
    const start = new Date(reservationSearchForm.elements.start_time.value);
    const end = new Date(reservationSearchForm.elements.end_time.value);
    if (Number.isNaN(start.getTime()) || Number.isNaN(end.getTime()) || end <= start) {
        throw new Error("Choose a valid end time later than the start time.");
    }
    if (start <= new Date()) {
        throw new Error("Reservation start time must be in the future.");
    }
    return { start_time: start.toISOString(), end_time: end.toISOString() };
}

function renderReservationResults(slots) {
    if (!reservationResults) return;
    reservationResults.replaceChildren();
    if (slots.length === 0) {
        const empty = document.createElement("p");
        empty.className = "muted";
        empty.textContent = "No compatible slots are available for this time. Try another area or time.";
        reservationResults.append(empty);
        return;
    }

    slots.forEach((slot) => {
        const card = document.createElement("article");
        card.className = "reservation-slot-card";
        const information = document.createElement("div");
        const title = document.createElement("strong");
        title.textContent = `${slot.area_name} · ${slot.slot_number}`;
        const description = document.createElement("p");
        description.textContent = `${slot.slot_type}${slot.is_ev ? " · EV charging" : ""} · ${slot.location}`;
        information.append(title, description);
        if (slot.hourly_rate !== undefined) {
            const price = document.createElement("p");
            price.textContent = `${new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR" }).format(slot.hourly_rate)} / hour`;
            information.append(price);
        }
        const button = document.createElement("button");
        button.type = "button";
        button.className = "refresh-button";
        button.dataset.slotId = slot.slot_id;
        button.textContent = "Reserve this slot";
        card.append(information, button);
        reservationResults.append(card);
    });
}

function renderReservations(reservations) {
    if (!reservationList) return;
    reservationList.replaceChildren();
    if (reservations.length === 0) {
        const empty = document.createElement("p");
        empty.className = "muted";
        empty.textContent = "You don't have any reservations yet.";
        reservationList.append(empty);
        return;
    }

    reservations.forEach((reservation) => {
        const card = document.createElement("article");
        card.className = "reservation-card";
        const details = document.createElement("div");
        const title = document.createElement("strong");
        title.textContent = `${reservation.area_name} · ${reservation.slot_number}`;
        const description = document.createElement("p");
        description.textContent = `${reservation.registration_number} · ${formatIsoTimestamp(reservation.start_time)} – ${formatIsoTimestamp(reservation.end_time)}`;
        const status = document.createElement("span");
        status.className = "reservation-status";
        status.textContent = reservation.status[0].toUpperCase() + reservation.status.slice(1);
        details.append(title, description, status);
        const actions = document.createElement("div");
        actions.className = "reservation-actions";
        const viewButton = document.createElement("button");
        viewButton.type = "button";
        viewButton.className = "vehicle-cancel";
        viewButton.dataset.action = "details";
        viewButton.dataset.reservationId = reservation.reservation_id;
        viewButton.textContent = "Details";
        actions.append(viewButton);
        if (reservation.status === "pending" || reservation.status === "confirmed") {
            if (reservation.status === "confirmed") {
                const checkInButton = document.createElement("button");
                checkInButton.type = "button";
                checkInButton.className = "refresh-button";
                checkInButton.dataset.action = "check-in";
                checkInButton.dataset.reservationId = reservation.reservation_id;
                checkInButton.textContent = "Check in";
                actions.append(checkInButton);
            }
            const cancelButton = document.createElement("button");
            cancelButton.type = "button";
            cancelButton.className = "vehicle-cancel";
            cancelButton.dataset.action = "cancel";
            cancelButton.dataset.reservationId = reservation.reservation_id;
            cancelButton.textContent = "Cancel";
            actions.append(cancelButton);
        }
        card.append(details, actions);
        reservationList.append(card);
    });
}

function renderActiveParkingSessions(sessions) {
    if (!activeParkingSessions) return;
    activeParkingSessions.replaceChildren();
    if (sessions.length === 0) {
        const empty = document.createElement("p");
        empty.className = "muted";
        empty.textContent = "You don't have any active parking sessions.";
        activeParkingSessions.append(empty);
        return;
    }

    sessions.forEach((session) => {
        const card = document.createElement("article");
        card.className = "reservation-card";
        const details = document.createElement("div");
        const title = document.createElement("strong");
        title.textContent = `${session.area_name} · ${session.slot_number}`;
        const description = document.createElement("p");
        description.textContent = `${session.registration_number} · Entry ${formatIsoTimestamp(session.entry_time)} · Reserved until ${formatIsoTimestamp(session.reserved_end_time)}`;
        const status = document.createElement("span");
        status.className = "reservation-status";
        status.textContent = "Active";
        details.append(title, description, status);
        card.append(details);
        activeParkingSessions.append(card);
    });
}

async function loadReservations() {
    if (!reservationList) return;
    reservationList.replaceChildren();
    const loading = document.createElement("p");
    loading.className = "muted";
    loading.textContent = "Loading reservations...";
    reservationList.append(loading);
    try {
        const response = await fetch(`${API_BASE}/api/reservations`, { headers: authHeaders() });
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.reservations)) {
            throw new Error(data.error?.message || "Reservations could not be loaded.");
        }
        renderReservations(data.reservations);
    } catch (error) {
        reservationList.replaceChildren();
        setReservationMessage(error.message, true);
    }
}

async function loadActiveParkingSessions() {
    if (!activeParkingSessions) return;
    try {
        const response = await fetch(`${API_BASE}/api/parking-sessions/active`, {
            headers: authHeaders()
        });
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.sessions)) {
            throw new Error(data.error?.message || "Active parking sessions could not be loaded.");
        }
        renderActiveParkingSessions(data.sessions);
    } catch (error) {
        activeParkingSessions.replaceChildren();
        const message = document.createElement("p");
        message.className = "muted";
        message.textContent = error.message;
        activeParkingSessions.append(message);
    }
}

async function loadReservationPage() {
    setReservationDateDefaults();
    await Promise.all([loadReservationOptions(), loadReservations(), loadActiveParkingSessions()]);
}

if (reservationSearchForm) {
    reservationVehicle.addEventListener("change", () => {
        reservationSearchForm.elements.ev_only.checked = false;
        setReservationMessage("");
    });

    reservationSearchForm.addEventListener("invalid", (event) => {
        const field = event.target;
        const messages = {
            vehicle_id: "Log in and select one of your vehicles before searching.",
            start_time: "Choose a start time for the reservation.",
            end_time: "Choose an end time for the reservation."
        };
        setReservationMessage(
            messages[field.name] || "Complete the required search fields before searching.",
            true
        );
    }, true);

    reservationSearchForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        setReservationMessage("");
        try {
            const times = reservationSearchTimes();
            const parameters = new URLSearchParams({
                vehicle_id: reservationVehicle.value,
                start_time: times.start_time,
                end_time: times.end_time
            });
            if (reservationArea.value) parameters.set("area_id", reservationArea.value);
            const location = reservationSearchForm.elements.location.value.trim();
            const slotType = reservationSearchForm.elements.slot_type.value.trim();
            if (location) parameters.set("location", location);
            if (slotType) parameters.set("slot_type", slotType);
            if (reservationSearchForm.elements.ev_only.checked) parameters.set("ev_only", "true");

            const response = await fetch(
                `${API_BASE}/api/reservations/availability?${parameters.toString()}`,
                { headers: authHeaders() }
            );
            const data = await response.json();
            if (!response.ok || !Array.isArray(data.slots)) {
                throw new Error(data.error?.message || "Parking availability could not be searched.");
            }
            renderReservationResults(data.slots);
        } catch (error) {
            setReservationMessage(error.message, true);
        }
    });
}

if (reservationResults) {
    reservationResults.addEventListener("click", async (event) => {
        const button = event.target.closest("button[data-slot-id]");
        if (!button) return;
        setReservationMessage("");
        try {
            const times = reservationSearchTimes();
            const response = await fetch(`${API_BASE}/api/reservations`, {
                method: "POST",
                headers: { ...authHeaders(), "Content-Type": "application/json" },
                body: JSON.stringify({
                    vehicle_id: Number(reservationVehicle.value),
                    slot_id: Number(button.dataset.slotId),
                    ...times
                })
            });
            const data = await response.json();
            if (!response.ok || !data.reservation) {
                throw new Error(data.error?.message || "The reservation could not be created.");
            }
            setReservationMessage(`Reservation #${data.reservation.reservation_id} confirmed for ${data.reservation.slot_number}.`);
            await loadReservations();
            reservationSearchForm.requestSubmit();
        } catch (error) {
            setReservationMessage(error.message, true);
        }
    });
}

if (reservationList) {
    reservationList.addEventListener("click", async (event) => {
        const button = event.target.closest("button[data-action]");
        if (!button) return;
        const reservationId = encodeURIComponent(button.dataset.reservationId);
        try {
            if (button.dataset.action === "check-in") {
                button.disabled = true;
                const response = await fetch(`${API_BASE}/api/reservations/${reservationId}/check-in`, {
                    method: "POST",
                    headers: authHeaders()
                });
                const data = await response.json();
                if (!response.ok || !data.session) {
                    throw new Error(data.error?.message || "The reservation could not be checked in.");
                }
                setReservationMessage(
                    `Checked in successfully. ${data.session.registration_number} is parked at ${data.session.area_name}, ${data.session.slot_number}.`
                );
                await Promise.all([loadReservations(), loadActiveParkingSessions()]);
                return;
            }
            if (button.dataset.action === "details") {
                const response = await fetch(`${API_BASE}/api/reservations/${reservationId}`, {
                    headers: authHeaders()
                });
                const data = await response.json();
                if (!response.ok || !data.reservation) {
                    throw new Error(data.error?.message || "Reservation details could not be loaded.");
                }
                const reservation = data.reservation;
                const rate = reservation.hourly_rate === undefined
                    ? "Rate not configured"
                    : `${new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR" }).format(reservation.hourly_rate)} per hour`;
                setReservationMessage(
                    `Reservation #${reservation.reservation_id}: ${reservation.area_name}, ${reservation.slot_number}; ${reservation.status}; ${rate}.`
                );
                return;
            }
            const response = await fetch(`${API_BASE}/api/reservations/${reservationId}`, {
                method: "DELETE",
                headers: authHeaders()
            });
            const data = response.status === 204 ? {} : await response.json();
            if (!response.ok) {
                throw new Error(data.error?.message || "The reservation could not be cancelled.");
            }
            setReservationMessage(`Reservation #${reservationId} cancelled.`);
            await loadReservations();
            reservationSearchForm.requestSubmit();
        } catch (error) {
            if (button.dataset.action === "check-in") {
                button.disabled = false;
            }
            setReservationMessage(error.message, true);
        }
    });
}

if (refreshReservationsButton) {
    refreshReservationsButton.addEventListener("click", () => {
        Promise.all([loadReservations(), loadActiveParkingSessions()]);
    });
}
