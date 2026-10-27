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
const parkingHistoryList = document.getElementById("parkingHistoryList");
const notificationList = document.getElementById("notificationList");
const adminNotificationList = document.getElementById("adminNotificationList");
const popoverNotificationList = document.getElementById("popoverNotificationList");
const notificationUnreadBadge = document.getElementById("notificationUnreadBadge");
const notificationUnreadLabel = document.getElementById("notificationUnreadLabel");
const popoverUnreadLabel = document.getElementById("popoverUnreadLabel");
const notificationPopover = document.getElementById("notificationPopover");
const notificationsButton = document.getElementById("notificationsButton");
const historyPageLabel = document.getElementById("historyPageLabel");
const notificationPageLabel = document.getElementById("notificationPageLabel");
const adminNotificationPageLabel = document.getElementById("adminNotificationPageLabel");
const historyPageSize = 10;
const notificationPageSize = 10;
let historyPageOffset = 0;
let notificationPageOffset = 0;

function setReservationMessage(message, isError = false) {
    if (reservationMessage) {
        reservationMessage.textContent = message;
        reservationMessage.classList.toggle("is-error", isError);
    }
    if (message && typeof showToast === "function") {
        showToast(message, isError ? "error" : "success");
    }
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

        const actions = document.createElement("div");
        actions.className = "reservation-actions";

        const feeBtn = document.createElement("button");
        feeBtn.type = "button";
        feeBtn.className = "vehicle-cancel";
        feeBtn.dataset.action = "fee-summary";
        feeBtn.dataset.sessionId = session.session_id;
        feeBtn.textContent = "Fee summary";

        const exitBtn = document.createElement("button");
        exitBtn.type = "button";
        exitBtn.className = "refresh-button";
        exitBtn.dataset.action = "exit";
        exitBtn.dataset.sessionId = session.session_id;
        exitBtn.textContent = "Check out / Exit";

        actions.append(feeBtn, exitBtn);
        card.append(details, actions);
        activeParkingSessions.append(card);
    });
}

function renderParkingHistory(items) {
    if (!parkingHistoryList) return;
    parkingHistoryList.replaceChildren();
    if (items.length === 0) {
        const empty = document.createElement("p");
        empty.className = "muted";
        empty.textContent = "Completed, cancelled, or expired parking activity will appear here.";
        parkingHistoryList.append(empty);
        return;
    }

    items.forEach((item) => {
        const card = document.createElement("article");
        card.className = "reservation-card history-card";
        const details = document.createElement("div");
        const title = document.createElement("strong");
        title.textContent = `${item.area_name} · ${item.slot_number}`;
        const description = document.createElement("p");
        description.textContent = `${item.registration_number} · ${formatIsoTimestamp(item.start_time)} – ${formatIsoTimestamp(item.end_time)}`;
        const status = document.createElement("span");
        status.className = "reservation-status";
        status.textContent = item.reservation_status[0].toUpperCase() + item.reservation_status.slice(1);
        details.append(title, description, status);

        if (item.fee_summary) {
            const fee = document.createElement("p");
            const summary = item.fee_summary;
            fee.className = "history-fee-summary";
            fee.textContent = `Session ${summary.status} · ${summary.duration_minutes} min · ${new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR" }).format(summary.total_fee)} · ${summary.payment_status.replace(/_/g, " ")}`;
            details.append(fee);
        }
        card.append(details);
        parkingHistoryList.append(card);
    });
}

async function loadParkingHistory() {
    if (!parkingHistoryList) return;
    try {
        const response = await fetch(
            `${API_BASE}/api/history?limit=${historyPageSize}&offset=${historyPageOffset}`,
            { headers: authHeaders() }
        );
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.history)) {
            throw new Error(data.error?.message || "Parking history could not be loaded.");
        }
        renderParkingHistory(data.history);
        const total = Number(data.total) || 0;
        if (total === 0) {
            historyPageOffset = 0;
        } else if (historyPageOffset >= total) {
            historyPageOffset = Math.floor((total - 1) / historyPageSize) * historyPageSize;
        }
        if (historyPageOffset !== data.offset) {
            await loadParkingHistory();
            return;
        }
        if (historyPageLabel) {
            historyPageLabel.textContent = `Page ${Math.floor(historyPageOffset / historyPageSize) + 1}`;
        }
        const previous = document.getElementById("previousHistoryPage");
        const next = document.getElementById("nextHistoryPage");
        if (previous) previous.disabled = historyPageOffset === 0;
        if (next) next.disabled = historyPageOffset + historyPageSize >= total;
    } catch (error) {
        parkingHistoryList.replaceChildren();
        const message = document.createElement("p");
        message.className = "muted";
        message.textContent = error.message;
        parkingHistoryList.append(message);
    }
}

function renderNotifications(items) {
    const renderList = (list, compact = false) => {
        if (!list) return;
        list.replaceChildren();
        if (items.length === 0) {
            const empty = document.createElement("p");
            empty.className = "muted";
            empty.textContent = "You have no notifications.";
            list.append(empty);
            return;
        }

        (compact ? items.slice(0, 5) : items).forEach((item) => {
            const card = document.createElement("article");
            card.className = `${compact ? "popover-notification" : "reservation-card notification-card"}${item.is_read ? " is-read" : " is-unread"}`;
            const details = document.createElement("div");
            const title = document.createElement("strong");
            title.textContent = item.title;
            const message = document.createElement("p");
            message.textContent = item.message;
            const created = document.createElement("span");
            created.className = "muted notification-created";
            created.textContent = formatIsoTimestamp(item.created_at);
            details.append(title, message, created);
            card.append(details);
            if (!item.is_read) {
                const readButton = document.createElement("button");
                readButton.type = "button";
                readButton.className = "vehicle-cancel";
                readButton.dataset.notificationReadId = item.notification_id;
                readButton.textContent = compact ? "Read" : "Mark read";
                card.append(readButton);
            }
            const deleteButton = document.createElement("button");
            deleteButton.type = "button";
            deleteButton.className = "vehicle-cancel notification-delete-button";
            deleteButton.dataset.notificationDeleteId = item.notification_id;
            deleteButton.textContent = "Delete";
            deleteButton.setAttribute("aria-label", `Delete notification: ${item.title}`);
            card.append(deleteButton);
            list.append(card);
        });
    };
    renderList(notificationList);
    renderList(adminNotificationList);
    renderList(popoverNotificationList, true);
}

async function loadNotifications() {
    if (!notificationList && !adminNotificationList && !popoverNotificationList) return;
    try {
        const response = await fetch(
            `${API_BASE}/api/notifications?limit=${notificationPageSize}&offset=${notificationPageOffset}`,
            { headers: authHeaders() }
        );
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.notifications)) {
            throw new Error(data.error?.message || "Notifications could not be loaded.");
        }
        if (data.notifications.length === 0 && notificationPageOffset > 0 && notificationPageOffset >= Number(data.total)) {
            const lastPageOffset = Math.floor(Math.max(0, Number(data.total) - 1) / notificationPageSize) * notificationPageSize;
            notificationPageOffset = Math.min(notificationPageOffset, lastPageOffset);
            return loadNotifications();
        }
        renderNotifications(data.notifications);
        const unreadCount = Number(data.unread_count) || 0;
        if (notificationUnreadBadge) {
            notificationUnreadBadge.textContent = String(unreadCount);
            notificationUnreadBadge.hidden = unreadCount === 0;
        }
        if (notificationUnreadLabel) {
            notificationUnreadLabel.textContent = `(${unreadCount} unread)`;
        }
        const adminUnreadLabel = document.getElementById("adminNotificationUnreadLabel");
        if (adminUnreadLabel) adminUnreadLabel.textContent = `(${unreadCount} unread)`;
        if (popoverUnreadLabel) {
            popoverUnreadLabel.textContent = `${unreadCount} unread`;
        }
        if (notificationPageLabel) {
            notificationPageLabel.textContent = `Page ${Math.floor(notificationPageOffset / notificationPageSize) + 1}`;
        }
        if (adminNotificationPageLabel) {
            adminNotificationPageLabel.textContent = `Page ${Math.floor(notificationPageOffset / notificationPageSize) + 1}`;
        }
        const previous = document.getElementById("previousNotificationPage");
        const next = document.getElementById("nextNotificationPage");
        const adminPrevious = document.getElementById("adminPreviousNotificationPage");
        const adminNext = document.getElementById("adminNextNotificationPage");
        [previous, adminPrevious].forEach((button) => { if (button) button.disabled = notificationPageOffset === 0; });
        [next, adminNext].forEach((button) => { if (button) button.disabled = notificationPageOffset + notificationPageSize >= Number(data.total); });
    } catch (error) {
        const message = document.createElement("p");
        message.className = "muted";
        message.textContent = error.message;
        [notificationList, adminNotificationList, popoverNotificationList].forEach((list) => {
            if (!list) return;
            list.replaceChildren(message.cloneNode(true));
        });
    }
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
    await Promise.all([
        loadReservationOptions(),
        loadReservations(),
        loadActiveParkingSessions(),
        loadParkingHistory(),
        loadNotifications()
    ]);
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
            const r = data.reservation;
            setReservationMessage(`Reservation #${r.reservation_id} confirmed for ${r.slot_number}.`);
            await showAppNotice({
                title: "Reservation Confirmed! 🎫",
                message: `Your parking slot has been successfully reserved at ${r.area_name}.`,
                type: "success",
                subtitle: `Reservation #${r.reservation_id}`,
                details: [
                    { label: "Parking Slot", value: `${r.area_name} · Slot ${r.slot_number}` },
                    { label: "Reserved From", value: formatIsoTimestamp(r.start_time) },
                    { label: "Reserved Until", value: formatIsoTimestamp(r.end_time) },
                    { label: "Check-In Window", value: "Start time + 5 min grace (auto-cancelled if >5 min late)" },
                    { label: "Overtime Policy", value: "7 min grace after end time, then ₹1/min" }
                ],
                confirmText: "Got It"
            });
            await Promise.all([loadReservations(), loadNotifications()]);
            reservationSearchForm.requestSubmit();
        } catch (error) {
            setReservationMessage(error.message, true);
            await showAppNotice({
                title: "Reservation Failed",
                message: error.message,
                type: "danger",
                confirmText: "Close"
            });
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
                    const err = new Error(data.error?.message || "The reservation could not be checked in.");
                    err.code = data.error?.code;
                    throw err;
                }
                const sess = data.session;
                setReservationMessage(
                    `Checked in successfully. ${sess.registration_number} is parked at ${sess.area_name}, ${sess.slot_number}.`
                );
                await showAppNotice({
                    title: "Checked In Successfully! 🎉",
                    message: `${sess.registration_number} is now officially checked in at ${sess.area_name}, Slot ${sess.slot_number}.`,
                    type: "success",
                    subtitle: `Active Session #${sess.session_id}`,
                    details: [
                        { label: "Vehicle", value: sess.registration_number },
                        { label: "Parking Slot", value: `${sess.area_name} · Slot ${sess.slot_number}` },
                        { label: "Check-In Time", value: formatIsoTimestamp(sess.entry_time) },
                        { label: "Reserved End Time", value: formatIsoTimestamp(sess.reserved_end_time) },
                        { label: "Overtime Rule", value: "7 min free grace period, then ₹1/min" }
                    ],
                    confirmText: "Proceed"
                });
                await Promise.all([loadReservations(), loadActiveParkingSessions(), loadNotifications()]);
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
                await showAppNotice({
                    title: `Reservation #${reservation.reservation_id}`,
                    message: `Status: ${reservation.status.toUpperCase()}`,
                    type: "info",
                    details: [
                        { label: "Location", value: `${reservation.area_name} · Slot ${reservation.slot_number}` },
                        { label: "Start Time", value: formatIsoTimestamp(reservation.start_time) },
                        { label: "End Time", value: formatIsoTimestamp(reservation.end_time) },
                        { label: "Rate", value: rate }
                    ],
                    confirmText: "Close"
                });
                return;
            }
            if (button.dataset.action === "cancel") {
                const confirmed = await showAppConfirm({
                    title: "Cancel Reservation?",
                    message: `Are you sure you want to cancel Reservation #${reservationId}? The slot will be released for other drivers.`,
                    type: "warning",
                    confirmText: "Yes, Cancel Reservation",
                    cancelText: "Keep Reservation"
                });
                if (!confirmed) return;

                const response = await fetch(`${API_BASE}/api/reservations/${reservationId}`, {
                    method: "DELETE",
                    headers: authHeaders()
                });
                const data = response.status === 204 ? {} : await response.json();
                if (!response.ok) {
                    throw new Error(data.error?.message || "The reservation could not be cancelled.");
                }
                setReservationMessage(`Reservation #${reservationId} cancelled.`);
                await showAppNotice({
                    title: "Reservation Cancelled",
                    message: `Reservation #${reservationId} has been cancelled successfully.`,
                    type: "info",
                    confirmText: "OK"
                });
                await Promise.all([loadReservations(), loadParkingHistory(), loadNotifications()]);
                reservationSearchForm.requestSubmit();
            }
        } catch (error) {
            if (button.dataset.action === "check-in") {
                button.disabled = false;
                let noticeTitle = "Check-In Not Available";
                let noticeType = "warning";
                let causeExplanation = error.message;

                if (error.code === "CHECK_IN_NOT_OPEN") {
                    noticeTitle = "Check-In Not Started Yet ⏰";
                    noticeType = "warning";
                    causeExplanation = "Your reservation start time has not arrived yet. Check-in opens exactly at the scheduled start time. You will have a 5-minute grace period after start time to check in.";
                } else if (error.code === "CHECK_IN_GRACE_PERIOD_EXCEEDED") {
                    noticeTitle = "Reservation Cancelled ⏰";
                    noticeType = "danger";
                    causeExplanation = "You checked in more than 5 minutes past your reservation start time. The 5-minute check-in grace period has expired, and your reservation has been cancelled. The parking slot has been released for other vehicles.";
                    await Promise.all([loadReservations(), loadNotifications()]);
                } else if (error.code === "RESERVATION_EXPIRED") {
                    noticeTitle = "Reservation Expired ⏳";
                    noticeType = "danger";
                    causeExplanation = "The scheduled end time for this reservation has already passed. The reservation has expired and the parking slot has been released.";
                    await Promise.all([loadReservations(), loadNotifications()]);
                } else if (error.code === "RESERVATION_NOT_CHECK_IN_ELIGIBLE") {
                    noticeTitle = "Reservation Ineligible for Check-In";
                    noticeType = "warning";
                    causeExplanation = "Only confirmed reservations can be checked in.";
                } else if (error.code === "PARKING_SLOT_UNAVAILABLE") {
                    noticeTitle = "Slot Unavailable";
                    noticeType = "danger";
                    causeExplanation = "The reserved parking slot is currently unavailable or undergoing maintenance.";
                } else if (error.code === "VEHICLE_ALREADY_PARKED") {
                    noticeTitle = "Vehicle Already Parked";
                    noticeType = "warning";
                    causeExplanation = "This vehicle is already checked in to another active parking session.";
                }

                await showAppNotice({
                    title: noticeTitle,
                    message: causeExplanation,
                    type: noticeType,
                    confirmText: "Understood"
                });
            } else {
                await showAppNotice({
                    title: "Notice",
                    message: error.message,
                    type: "danger"
                });
            }
            setReservationMessage(error.message, true);
        }
    });
}

if (refreshReservationsButton) {
    refreshReservationsButton.addEventListener("click", () => {
        Promise.all([
            loadReservations(),
            loadActiveParkingSessions(),
            loadParkingHistory(),
            loadNotifications()
        ]);
    });
}

if (notificationsButton) {
    notificationsButton.addEventListener("click", async (event) => {
        event.stopPropagation();
        const opening = notificationPopover.hidden;
        notificationPopover.hidden = !opening;
        notificationsButton.setAttribute("aria-expanded", String(opening));
        const accountPopover = document.getElementById("accountPopover");
        const accountButton = document.getElementById("accountButton");
        if (accountPopover && !accountPopover.hidden) {
            accountPopover.hidden = true;
            accountButton?.setAttribute("aria-expanded", "false");
        }
        if (opening) {
            notificationPageOffset = 0;
            await loadNotifications();
        }
    });
}

document.getElementById("viewAllNotifications")?.addEventListener("click", () => {
    if (notificationPopover) notificationPopover.hidden = true;
    notificationsButton?.setAttribute("aria-expanded", "false");
    notificationPageOffset = 0;
    if (typeof adminConsole !== "undefined" && adminConsole && !adminConsole.hidden) {
        const notificationsTab = document.querySelector('.admin-nav-tab[data-admin-tab="notifications"]');
        notificationsTab?.click();
    } else {
        if (typeof showDashboardPage === "function") showDashboardPage("notifications");
        loadNotifications();
    }
});

document.addEventListener("click", (event) => {
    if (notificationPopover?.contains(event.target) || notificationsButton?.contains(event.target)) return;
    if (notificationPopover) notificationPopover.hidden = true;
    notificationsButton?.setAttribute("aria-expanded", "false");
});

document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape") return;
    if (notificationPopover) notificationPopover.hidden = true;
    notificationsButton?.setAttribute("aria-expanded", "false");
});

document.getElementById("previousHistoryPage")?.addEventListener("click", () => {
    historyPageOffset = Math.max(0, historyPageOffset - historyPageSize);
    loadParkingHistory();
});
document.getElementById("nextHistoryPage")?.addEventListener("click", () => {
    historyPageOffset += historyPageSize;
    loadParkingHistory();
});
document.getElementById("previousNotificationPage")?.addEventListener("click", () => {
    notificationPageOffset = Math.max(0, notificationPageOffset - notificationPageSize);
    loadNotifications();
});
document.getElementById("nextNotificationPage")?.addEventListener("click", () => {
    notificationPageOffset += notificationPageSize;
    loadNotifications();
});
document.getElementById("adminPreviousNotificationPage")?.addEventListener("click", () => {
    notificationPageOffset = Math.max(0, notificationPageOffset - notificationPageSize);
    loadNotifications();
});
document.getElementById("adminNextNotificationPage")?.addEventListener("click", () => {
    notificationPageOffset += notificationPageSize;
    loadNotifications();
});

if (notificationList || adminNotificationList || popoverNotificationList) {
    const handleNotificationAction = async (event) => {
        const button = event.target.closest("button[data-notification-read-id], button[data-notification-delete-id]");
        if (!button) return;
        button.disabled = true;
        try {
            const isDelete = button.hasAttribute("data-notification-delete-id");
            const id = isDelete ? button.dataset.notificationDeleteId : button.dataset.notificationReadId;
            const response = await fetch(`${API_BASE}/api/notifications/${encodeURIComponent(id)}${isDelete ? "" : "/read"}`, {
                method: isDelete ? "DELETE" : "POST",
                headers: authHeaders()
            });
            const data = await response.json();
            if (!response.ok || (isDelete ? data.deleted !== true : data.is_read !== true)) {
                throw new Error(data.error?.message || (isDelete ? "The notification could not be deleted." : "The notification could not be marked as read."));
            }
            if (isDelete && notificationPageOffset > 0 && notificationList?.children.length === 1) {
                notificationPageOffset = Math.max(0, notificationPageOffset - notificationPageSize);
            }
            await loadNotifications();
        } catch (error) {
            button.disabled = false;
            button.textContent = error.message;
        }
    };
    notificationList?.addEventListener("click", handleNotificationAction);
    adminNotificationList?.addEventListener("click", handleNotificationAction);
    popoverNotificationList?.addEventListener("click", handleNotificationAction);
}

async function markAllNotificationsAsRead(event) {
    const button = event.currentTarget;
    button.disabled = true;
    try {
        const response = await fetch(`${API_BASE}/api/notifications/read-all`, {
            method: "POST",
            headers: authHeaders()
        });
        const data = await response.json();
        if (!response.ok || data.unread_count !== 0) {
            throw new Error(data.error?.message || "Notifications could not be marked as read.");
        }
        await loadNotifications();
    } catch (error) {
        setReservationMessage(error.message, true);
    } finally {
        button.disabled = false;
    }
}
document.getElementById("markAllNotificationsRead")?.addEventListener("click", markAllNotificationsAsRead);
document.getElementById("adminMarkAllNotificationsRead")?.addEventListener("click", markAllNotificationsAsRead);
const sessionExitDialog = document.getElementById("sessionExitDialog");
const exitModalTitle = document.getElementById("exitModalTitle");
const exitModalBody = document.getElementById("exitModalBody");
const closeExitDialogButton = document.getElementById("closeExitDialogButton");

if (closeExitDialogButton && sessionExitDialog) {
    closeExitDialogButton.addEventListener("click", () => {
        sessionExitDialog.close();
    });
}

async function showSessionExitModal(sessionId, isCheckoutMode) {
    if (!sessionExitDialog || !exitModalBody) return;

    if (exitModalTitle) {
        exitModalTitle.textContent = isCheckoutMode
            ? "Confirm Vehicle Exit & Settle Fee"
            : "Live Session Fee Summary";
    }

    exitModalBody.replaceChildren();
    const loading = document.createElement("p");
    loading.className = "muted";
    loading.textContent = "Calculating current duration and fees...";
    exitModalBody.append(loading);

    sessionExitDialog.showModal();

    try {
        const response = await fetch(`${API_BASE}/api/parking-sessions/${encodeURIComponent(sessionId)}/fee-summary`, {
            headers: authHeaders()
        });
        const data = await response.json();
        if (!response.ok || !data.session) {
            throw new Error(data.error?.message || "Failed to load session fee summary.");
        }

        const summary = data.session;
        exitModalBody.replaceChildren();

        const card = document.createElement("div");
        card.className = "fee-breakdown-card";

        const headerRow = document.createElement("div");
        headerRow.className = "fee-breakdown-row";
        headerRow.innerHTML = `<strong>${summary.registration_number} (${summary.vehicle_type.toUpperCase()})</strong><span>${summary.area_name} · ${summary.slot_number}</span>`;
        card.append(headerRow);

        const timeRow = document.createElement("div");
        timeRow.className = "fee-breakdown-row";
        timeRow.innerHTML = `<span class="muted">Entry Time</span><span>${formatIsoTimestamp(summary.entry_time)}</span>`;
        card.append(timeRow);

        const reservedRow = document.createElement("div");
        reservedRow.className = "fee-breakdown-row";
        reservedRow.innerHTML = `<span class="muted">Reserved Until</span><span>${formatIsoTimestamp(summary.reserved_end_time)}</span>`;
        card.append(reservedRow);

        const durRow = document.createElement("div");
        durRow.className = "fee-breakdown-row";
        const bookedHours = Number(summary.booked_hours || summary.billed_hours || 1);
        durRow.innerHTML = `<span class="muted">Parking Duration</span><span>${summary.duration_minutes || 0} min (billed full ${bookedHours} hr)</span>`;
        card.append(durRow);

        const baseFeeRow = document.createElement("div");
        baseFeeRow.className = "fee-breakdown-row";
        baseFeeRow.innerHTML = `<span>Base Slot Fee (${bookedHours}h × ₹${Number(summary.hourly_rate || 0).toFixed(2)})</span><span>₹${Number(summary.base_fee || 0).toFixed(2)}</span>`;
        card.append(baseFeeRow);

        const overstayMins = Number(summary.overstay_duration_minutes) || 0;
        const billedOvertimeMins = Number(summary.billed_overtime_minutes != null ? summary.billed_overtime_minutes : summary.billed_overstay_minutes) || 0;
        const overtimeRate = Number(summary.overtime_rate_per_minute) || 1.0;
        const overtimeFee = Number(summary.overstay_fee) || 0.0;

        if (overstayMins > 0) {
            const overstayWarning = document.createElement("div");
            if (billedOvertimeMins > 0) {
                overstayWarning.className = "fee-overstay-warning";
                overstayWarning.textContent = `⚠ Overstayed reserved time by ${overstayMins} min (7 min free grace + ${billedOvertimeMins} billable min @ ₹${overtimeRate.toFixed(2)}/min)`;
                card.append(overstayWarning);

                const overstayFeeRow = document.createElement("div");
                overstayFeeRow.className = "fee-breakdown-row";
                overstayFeeRow.innerHTML = `<span>Overtime Fee (${billedOvertimeMins} min × ₹${overtimeRate.toFixed(2)}/min)</span><span>₹${overtimeFee.toFixed(2)}</span>`;
                card.append(overstayFeeRow);
            } else {
                overstayWarning.className = "fee-overstay-warning";
                overstayWarning.style.background = "var(--surface-subtle, #EAF6DD)";
                overstayWarning.style.color = "var(--color-primary, #064734)";
                overstayWarning.style.border = "1px solid var(--border-color, #D5E8C9)";
                overstayWarning.textContent = `ℹ Overstayed reserved time by ${overstayMins} min (within 7-minute free grace period — ₹0 overtime)`;
                card.append(overstayWarning);
            }
        }

        const totalRow = document.createElement("div");
        totalRow.className = "fee-breakdown-row total";
        totalRow.innerHTML = `<span>Total Fee</span><span>₹${Number(summary.total_fee || 0).toFixed(2)}</span>`;
        card.append(totalRow);

        exitModalBody.append(card);

        if (isCheckoutMode) {
            const paymentSec = document.createElement("div");
            paymentSec.className = "exit-payment-section";
            const label = document.createElement("label");
            label.htmlFor = "exitPaymentActionSelect";
            label.textContent = "Payment Option";

            const select = document.createElement("select");
            select.id = "exitPaymentActionSelect";
            select.innerHTML = `
                <option value="payment_at_desk">Payment at desk</option>
                <option value="online_payment">Online payment</option>
            `;
            paymentSec.append(label, select);
            exitModalBody.append(paymentSec);

            const actionSec = document.createElement("div");
            actionSec.className = "exit-modal-actions";

            const cancelBtn = document.createElement("button");
            cancelBtn.type = "button";
            cancelBtn.className = "vehicle-cancel";
            cancelBtn.textContent = "Cancel";
            cancelBtn.addEventListener("click", () => sessionExitDialog.close());

            const confirmBtn = document.createElement("button");
            confirmBtn.type = "button";
            confirmBtn.className = "refresh-button";
            confirmBtn.textContent = "Confirm Exit & Release Slot";
            confirmBtn.addEventListener("click", async () => {
                confirmBtn.disabled = true;
                confirmBtn.textContent = "Processing exit...";
                try {
                    const exitRes = await fetch(`${API_BASE}/api/parking-sessions/${encodeURIComponent(sessionId)}/exit`, {
                        method: "POST",
                        headers: {
                            ...authHeaders(),
                            "Content-Type": "application/json"
                        },
                        body: JSON.stringify({
                            payment_action: select.value
                        })
                    });
                    const exitData = await exitRes.json();
                    if (!exitRes.ok || !exitData.session) {
                        throw new Error(exitData.error?.message || "Failed to process vehicle exit.");
                    }
                    sessionExitDialog.close();
                    const s = exitData.session;
                    setReservationMessage(
                        `Vehicle ${s.registration_number} checked out successfully! Slot ${s.slot_number} has been released. Total fee: ₹${Number(s.total_fee).toFixed(2)} (${s.payment_status}).`
                    );
                    await showAppNotice({
                        title: "Check Out Complete! 🏁",
                        message: `Vehicle ${s.registration_number} checked out successfully and slot ${s.slot_number} has been released.`,
                        type: "success",
                        subtitle: `Session #${s.session_id}`,
                        details: [
                            { label: "Vehicle", value: s.registration_number },
                            { label: "Slot Released", value: `${s.area_name || ""} · ${s.slot_number}` },
                            { label: "Duration", value: formatDuration((s.duration_minutes || 0) * 60) },
                            { label: "Total Billed Fee", value: `₹${Number(s.total_fee).toFixed(2)}` },
                            { label: "Payment Status", value: (s.payment_status || "paid").replace(/_/g, " ").toUpperCase() }
                        ],
                        confirmText: "Done"
                    });
                    await Promise.all([
                        loadReservations(),
                        loadActiveParkingSessions(),
                        loadParkingHistory(),
                        loadNotifications()
                    ]);
                    if (typeof loadAreas === "function") {
                        await loadAreas();
                    }
                } catch (err) {
                    confirmBtn.disabled = false;
                    confirmBtn.textContent = "Confirm Exit & Release Slot";
                    await showAppNotice({
                        title: "Exit Processing Failed",
                        message: err.message,
                        type: "danger"
                    });
                }
            });

            actionSec.append(cancelBtn, confirmBtn);
            exitModalBody.append(actionSec);
        } else {
            const actionSec = document.createElement("div");
            actionSec.className = "exit-modal-actions";

            const closeBtn = document.createElement("button");
            closeBtn.type = "button";
            closeBtn.className = "vehicle-cancel";
            closeBtn.textContent = "Close";
            closeBtn.addEventListener("click", () => sessionExitDialog.close());

            const proceedBtn = document.createElement("button");
            proceedBtn.type = "button";
            proceedBtn.className = "refresh-button";
            proceedBtn.textContent = "Proceed to Check out";
            proceedBtn.addEventListener("click", () => {
                showSessionExitModal(sessionId, true);
            });

            actionSec.append(closeBtn, proceedBtn);
            exitModalBody.append(actionSec);
        }
    } catch (err) {
        exitModalBody.replaceChildren();
        const errP = document.createElement("p");
        errP.className = "muted";
        errP.textContent = err.message;
        exitModalBody.append(errP);
    }
}

if (activeParkingSessions) {
    activeParkingSessions.addEventListener("click", (event) => {
        const button = event.target.closest("button[data-action]");
        if (!button) return;
        const sessionId = button.dataset.sessionId;
        if (button.dataset.action === "fee-summary") {
            showSessionExitModal(sessionId, false);
        } else if (button.dataset.action === "exit") {
            showSessionExitModal(sessionId, true);
        }
    });
}
