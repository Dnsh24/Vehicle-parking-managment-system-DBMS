const API_BASE = "http://localhost:3000";
const authGate = document.getElementById("authGate");
const protectedContent = document.getElementById("protectedContent");
const loginScreen = document.getElementById("loginScreen");
const registerScreen = document.getElementById("registerScreen");
const loginForm = document.getElementById("loginForm");
const registerForm = document.getElementById("registerForm");
const registerSwitch = document.getElementById("registerSwitch");
const loginSwitch = document.getElementById("loginSwitch");
const authError = document.getElementById("authError");
const logoutButton = document.getElementById("logoutButton");
const topbar = document.getElementById("topbar");
const forgotPasswordButton = document.getElementById("forgotPasswordButton");
const passwordToggles = document.querySelectorAll("[data-password-toggle]");
const selectedSlot = document.getElementById("selectedSlot");
const selectedStatus = document.getElementById("selectedStatus");
const selectedVisual = document.getElementById("selectedVisual");
const detailStatus = document.getElementById("detailStatus");
const detailVehicle = document.getElementById("detailVehicle");
const detailType = document.getElementById("detailType");
const detailFeatures = document.getElementById("detailFeatures");
const detailRegistrationRow = document.getElementById("detailRegistrationRow");
const detailRegistration = document.getElementById("detailRegistration");
const detailDurationRow = document.getElementById("detailDurationRow");
const detailDuration = document.getElementById("detailDuration");
const parkingMap = document.getElementById("parkingMap");
const floorTitle = document.getElementById("floorTitle");
const availableCount = document.getElementById("availableCount");
const areaName = document.querySelector(".area-picker h2");
const floorTabs = document.querySelectorAll(".floor-tab");
const vehicleForm = document.getElementById("vehicleForm");
const vehicleList = document.getElementById("vehicleList");
const vehicleMessage = document.getElementById("vehicleMessage");
const saveVehicleButton = document.getElementById("saveVehicleButton");
const cancelVehicleEdit = document.getElementById("cancelVehicleEdit");

let areas = [];
let selectedArea = null;
let selectedParkedSeconds = 0;
let selectedDurationTimer = null;
let slotSelection = null;
function resetPasswordToggles(screen) {
    screen.querySelectorAll("[data-password-toggle]").forEach((toggle) => {
        const input = toggle.closest(".auth-input").querySelector("input");
        if (input) {
            input.type = "password";
        }
        toggle.textContent = "Show";
        toggle.setAttribute("aria-label", "Show password");
    });
}

function showAuthScreen(screenToShow, screenToHide) {
    screenToHide.hidden = true;
    void screenToHide.offsetWidth;
    screenToShow.hidden = false;
    void screenToShow.offsetWidth;
    resetPasswordToggles(screenToShow);
    showAuthError("");
    const firstInput = screenToShow.querySelector("input");
    if (firstInput) {
        firstInput.focus({ preventScroll: true });
    }
}

function showLoginScreen() {
    showAuthScreen(loginScreen, registerScreen);
}

function showRegisterScreen() {
    showAuthScreen(registerScreen, loginScreen);
}

function authHeaders() {
    const token = window.localStorage.getItem("parking_auth_token") || window.sessionStorage.getItem("parking_auth_token");
    return token ? { Authorization: `Bearer ${token}` } : {};
}

function showAuthenticatedUser(user) {
    authGate.setAttribute("hidden", "");
    protectedContent.removeAttribute("hidden");
    topbar.hidden = false;
    logoutButton.textContent = user.name.slice(0, 2).toUpperCase();
    loadAreas();
    loadVehicles();
}

function showAuthError(message) {
    authError.textContent = message;
}

passwordToggles.forEach((toggle) => {
    toggle.addEventListener("click", () => {
        const password = toggle.closest(".auth-input").querySelector("input");
        const visible = password.type === "text";
        password.type = visible ? "password" : "text";
        toggle.textContent = visible ? "Show" : "Hide";
        toggle.setAttribute("aria-label", visible ? "Show password" : "Hide password");
    });
});

async function authenticate(endpoint, form) {
    showAuthError("");
    const values = Object.fromEntries(new FormData(form).entries());
    try {
        let response;
        try {
            response = await fetch(`${API_BASE}${endpoint}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(values)
            });
        } catch (error) {
            throw new Error("Cannot connect to the backend. Start parking_backend on port 3000.");
        }
        const responseText = await response.text();
        let data;
        try {
            data = responseText ? JSON.parse(responseText) : {};
        } catch (error) {
            throw new Error(`Backend returned an invalid response (${response.status}).`);
        }
        if (!response.ok) {
            throw new Error(data.error?.message || "Authentication failed.");
        }
        const rememberMe = form.elements.remember_me?.checked ?? false;
        const tokenStorage = rememberMe ? window.localStorage : window.sessionStorage;
        const otherStorage = rememberMe ? window.sessionStorage : window.localStorage;
        otherStorage.removeItem("parking_auth_token");
        tokenStorage.setItem("parking_auth_token", data.token);
        showAuthenticatedUser(data.user);
    } catch (error) {
        showAuthError(error.message);
    }
}

loginForm.addEventListener("submit", (event) => {
    event.preventDefault();
    authenticate("/api/auth/login", loginForm);
});

registerForm.addEventListener("submit", (event) => {
    event.preventDefault();
    authenticate("/api/auth/register", registerForm);
});

registerSwitch.addEventListener("click", showRegisterScreen);
loginSwitch.addEventListener("click", showLoginScreen);
forgotPasswordButton.addEventListener("click", () => {
    showAuthError("Password reset is not available yet. Contact an administrator.");
});

logoutButton.addEventListener("click", async () => {
    const token = window.localStorage.getItem("parking_auth_token") || window.sessionStorage.getItem("parking_auth_token");
    if (token) {
        await fetch(`${API_BASE}/api/auth/logout`, {
            method: "POST",
            headers: authHeaders()
        }).catch(() => {});
    }
    window.localStorage.removeItem("parking_auth_token");
    window.sessionStorage.removeItem("parking_auth_token");
    window.location.reload();
});

async function restoreSession() {
    const token = window.localStorage.getItem("parking_auth_token") || window.sessionStorage.getItem("parking_auth_token");
    if (!token) {
        return;
    }
    try {
        const response = await fetch(`${API_BASE}/api/auth/me`, { headers: authHeaders() });
        const data = await response.json();
        if (response.ok && data.user) {
            showAuthenticatedUser(data.user);
        } else {
            window.localStorage.removeItem("parking_auth_token");
        }
    } catch (error) {
        showAuthError("The backend could not be reached. Please try again.");
    }
}

function statusLabel(status) {
    return status === "available" ? "Available" : status[0].toUpperCase() + status.slice(1);
}

function formatDuration(totalSeconds) {
    const seconds = Math.max(0, Math.floor(totalSeconds));
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    if (days > 0) {
        return `${days}d ${hours}h ${minutes}m`;
    }
    if (hours > 0) {
        return `${hours}h ${minutes}m`;
    }
    return `${minutes}m`;
}

function displayVehicleType(vehicleType, isEv) {
    if (isEv && vehicleType === "car") {
        return "EV car";
    }
    return vehicleType[0].toUpperCase() + vehicleType.slice(1);
}

function imageNameFor(vehicleType, isEv) {
    if (isEv) {
        return "ev-car-top-view.svg";
    }
    if (vehicleType === "car") {
        return "car-top-view.svg";
    }
    return `${vehicleType}-top-view.svg`;
}

function sectionForSlot(slot) {
    const slotType = slot.slot_type.toLowerCase();
    if (slot.vehicle_type === "truck") {
        return "truck";
    }
    if (slot.vehicle_type === "bike" || slot.vehicle_type === "scooter") {
        return "bike";
    }
    if (slot.is_ev) {
        return "ev-car";
    }
    if (slotType.includes("handicap") || slotType.includes("accessible")) {
        return "accessible-car";
    }
    return "car";
}

function renderMessage(title, message, action = "") {
    parkingMap.innerHTML = `<div class="map-message"><strong>${title}</strong><span>${message}</span>${action}</div>`;
}

function renderSelectedVisual(slot, statusClass, vehicleType) {
    const vehicleImage = slot.classList.contains("occupied")
        ? `<img class="selected-vehicle-image ${vehicleType}" src="assets/images/${imageNameFor(vehicleType, slot.classList.contains("ev"))}" alt="${displayVehicleType(vehicleType, slot.classList.contains("ev"))}">`
        : "";
    const marker = vehicleImage || "<span aria-hidden=\"true\">♢</span>";

    selectedVisual.className = `selected-slot-visual ${statusClass}`;
    selectedVisual.innerHTML = `${marker}<strong>${slot.dataset.slot}</strong>`;
}

function selectSlot(slot) {
    const [vehicle, type, status] = slot.dataset.info.split(" · ");
    const statusClass = slot.classList.contains("available") ? "available" : slot.classList[1];
    selectedSlot.textContent = slot.dataset.slot;
    selectedStatus.textContent = status;
    selectedStatus.className = `status-pill ${statusClass}`;
    renderSelectedVisual(slot, statusClass, slot.dataset.vehicle);
    detailStatus.textContent = status === "Available" ? "Available to use" : status;
    const actualVehicleType = slot.dataset.vehicleRegistration
        ? slot.dataset.vehicle
        : null;
    detailVehicle.textContent = actualVehicleType
        ? displayVehicleType(actualVehicleType, slot.dataset.vehicleElectric === "true")
        : vehicle;
    detailType.textContent = type;
    const registration = slot.dataset.vehicleRegistration || "";
    detailRegistrationRow.hidden = !registration;
    detailRegistration.textContent = registration;
    detailDurationRow.hidden = !registration || !slot.dataset.parkedAt;
    if (selectedDurationTimer !== null) {
        window.clearInterval(selectedDurationTimer);
        selectedDurationTimer = null;
    }
    selectedParkedSeconds = Number(slot.dataset.parkedSeconds || 0);
    if (registration && slot.dataset.parkedAt) {
        const startedAt = Date.parse(slot.dataset.parkedAt);
        const updateDuration = () => {
            const liveSeconds = Number.isNaN(startedAt)
                ? selectedParkedSeconds
                : Math.floor((Date.now() - startedAt) / 1000);
            detailDuration.textContent = formatDuration(Math.max(selectedParkedSeconds, liveSeconds));
        };
        updateDuration();
        selectedDurationTimer = window.setInterval(updateDuration, 30000);
    }
    detailFeatures.innerHTML = slot.classList.contains("ev")
        ? '<span class="feature-tag">⚡ EV charging</span>'
        : slot.classList.contains("accessible")
            ? '<span class="feature-tag">♿ Accessible</span>'
            : '<span class="muted">Standard</span>';
}

function renderFloor(area, slots) {
    const groupedSlots = {
        truck: [],
        bike: [],
        car: [],
        "ev-car": [],
        "accessible-car": []
    };

    slots.forEach((slot) => groupedSlots[sectionForSlot(slot)].push(slot));
    const sections = [
        ["truck", "Truck / oversized"],
        ["bike", "Bike & scooter parking"],
        ["car", "Normal car parking"],
        ["ev-car", "EV charging"],
        ["accessible-car", "Handicapped parking"]
    ];

    floorTitle.textContent = area.name;
    areaName.textContent = area.name;
    const availableSlots = slots.filter((slot) => slot.status === "available").length;
    availableCount.innerHTML = `${availableSlots} <small>of ${area.capacity} slots</small>`;
    parkingMap.innerHTML = `<div class="map-edge entrance"><span>↓</span> ENTRANCE / RAMP</div>
        ${sections.map(([sectionClass, sectionTitle]) => `<section class="category-section ${sectionClass}">
            <div class="category-title">${sectionTitle}<span>${groupedSlots[sectionClass].length} slots</span></div>
            <div class="map-row">${groupedSlots[sectionClass].map((slot) => {
            const label = statusLabel(slot.status);
            const vehicleType = displayVehicleType(slot.vehicle_type, slot.is_ev);
            const slotType = slot.slot_type;
            const classes = `${slot.status}${slot.is_ev ? " ev" : ""}${sectionClass === "accessible-car" ? " accessible" : ""}`;
            const parked = Boolean(slot.vehicle_registration);
            const visibleVehicleType = parked ? slot.parked_vehicle_type : slot.vehicle_type;
            const vehicleImage = parked
                ? `<img class="vehicle-image ${visibleVehicleType}" src="assets/images/${imageNameFor(visibleVehicleType, slot.is_ev)}" alt="" aria-hidden="true">`
                : "";
            const accessibility = sectionClass === "accessible-car" ? "♿ " : "";
            const ev = slot.is_ev ? "EV · " : "";
            const slotContents = parked
                ? `<b>${slot.slot_number}</b><small class="slot-registration"></small>`
                : `<b>${slot.slot_number}</b><small>${accessibility}${ev}${label}</small>`;
            return `<div class="slot ${classes}" data-slot-id="${slot.slot_id}" data-slot="${slot.slot_number}" data-vehicle="${slot.vehicle_type}" data-info="${vehicleType} · ${slotType} · ${label}" tabindex="0">${vehicleImage}${slotContents}</div>`;
        }).join("")}</div></section><div class="lane"><span>←</span> DRIVE LANE <span>→</span></div>`).join("")}
        <div class="map-edge exit">EXIT / RAMP <span>↓</span></div>`;

    slots.forEach((slot) => {
        const element = parkingMap.querySelector(`[data-slot-id="${CSS.escape(String(slot.slot_id))}"]`);
        if (!element) {
            return;
        }
        if (slot.vehicle_registration) {
            element.dataset.vehicleRegistration = slot.vehicle_registration;
            element.dataset.parkedAt = slot.parked_at || "";
            element.dataset.parkedSeconds = String(slot.parked_seconds || 0);
            element.dataset.vehicle = slot.parked_vehicle_type || slot.vehicle_type;
            element.dataset.vehicleElectric = String(slot.parked_vehicle_is_electric);
            element.title = `${slot.vehicle_registration} · ${slot.slot_type}`;
            element.querySelector(".slot-registration").textContent = slot.vehicle_registration;
        } else {
            element.title = `${slot.slot_number} · ${statusLabel(slot.status)}`;
        }
    });
    applySlotSelection();

    parkingMap.querySelectorAll(".slot").forEach((slot) => {
        slot.addEventListener("click", () => {
            if (slot.getAttribute("aria-disabled") === "true") {
                return;
            }
            if (slot.dataset.selectVehicleId) {
                parkVehicleInSlot(slot.dataset.selectVehicleId, slot.dataset.slotId);
                return;
            }
            selectSlot(slot);
        });
        slot.addEventListener("keydown", (event) => {
            if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                if (slot.getAttribute("aria-disabled") === "true") {
                    return;
                }
                if (slot.dataset.selectVehicleId) {
                    parkVehicleInSlot(slot.dataset.selectVehicleId, slot.dataset.slotId);
                    return;
                }
                selectSlot(slot);
            }
        });
    });
}

async function loadFloor(area) {
    selectedArea = area;
    floorTitle.textContent = `Loading ${area.name}`;
    renderMessage("Loading parking layout", "Fetching the latest slot availability...");

    try {
        const response = await fetch(`${API_BASE}/api/parking-areas/${area.area_id}/slots`);
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.slots)) {
            throw new Error(data.error?.message || "Parking slots could not be loaded.");
        }
        if (data.slots.length === 0) {
            renderMessage("No slots configured", "This parking floor does not have any configured slots.");
            return;
        }
        renderFloor(area, data.slots);
    } catch (error) {
        renderMessage("Parking layout unavailable", error.message, '<button class="map-retry" type="button">Try again</button>');
        parkingMap.querySelector(".map-retry").addEventListener("click", () => loadFloor(area));
    }
}

function setFloorTab(area, index) {
    const tab = floorTabs[index];
    if (!tab) {
        return;
    }
    tab.hidden = false;
    tab.dataset.areaId = area.area_id;
    tab.textContent = area.name.replace("Mall Central — ", "").replace("Mall Central - ", "");
    const badge = document.createElement("span");
    badge.textContent = index === 0 ? "G" : String(index);
    tab.appendChild(badge);
    tab.classList.toggle("active", index === 0);
    tab.addEventListener("click", () => {
        floorTabs.forEach((item) => item.classList.remove("active"));
        tab.classList.add("active");
        loadFloor(area);
    });
}

async function loadAreas() {
    renderMessage("Loading parking floors", "Fetching Mall Central parking areas...");
    try {
        const response = await fetch(`${API_BASE}/api/parking-areas`);
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.areas)) {
            throw new Error(data.error?.message || "Parking areas could not be loaded.");
        }
        areas = data.areas;
        if (areas.length === 0) {
            renderMessage("No parking floors available", "Mall Central has no active parking areas configured.");
            return;
        }
        floorTabs.forEach((tab, index) => {
            tab.hidden = index >= areas.length;
            if (areas[index]) {
                setFloorTab(areas[index], index);
            }
        });
        await loadFloor(areas[0]);
    } catch (error) {
        renderMessage("Parking information unavailable", error.message, '<button class="map-retry" type="button">Try again</button>');
        parkingMap.querySelector(".map-retry").addEventListener("click", loadAreas);
    }
}

document.getElementById("refreshButton").addEventListener("click", (event) => {
    const button = event.currentTarget;
    button.innerHTML = '<span aria-hidden="true">↻</span> Updated';
    if (selectedArea) {
        loadFloor(selectedArea);
    }
    window.setTimeout(() => {
        button.innerHTML = '<span aria-hidden="true">↻</span> Refresh';
    }, 1200);
});

document.getElementById("changeAreaButton").addEventListener("click", () => {
    window.alert("Mall Central floors are available in the floor selector.");
});

function setVehicleMessage(message, isError = false) {
    vehicleMessage.textContent = message;
    vehicleMessage.classList.toggle("is-error", isError);
}

function resetVehicleForm() {
    vehicleForm.reset();
    vehicleForm.elements.vehicle_id.value = "";
    saveVehicleButton.textContent = "Add vehicle";
    cancelVehicleEdit.hidden = true;
}

function renderVehicles(vehicles) {
    vehicleList.replaceChildren();
    if (vehicles.length === 0) {
        const empty = document.createElement("p");
        empty.className = "muted";
        empty.textContent = "You haven't added a vehicle yet.";
        vehicleList.append(empty);
        return;
    }

    vehicles.forEach((vehicle) => {
        const item = document.createElement("article");
        item.className = "vehicle-card";
        item.dataset.vehicleId = vehicle.vehicle_id;
        const details = document.createElement("div");
        const registration = document.createElement("strong");
        registration.textContent = vehicle.registration_number;
        const description = document.createElement("p");
        const type = vehicle.vehicle_type[0].toUpperCase() + vehicle.vehicle_type.slice(1);
        description.textContent = `${type}${vehicle.is_electric ? " · Electric" : ""}`;
        details.append(registration, description);
        if (vehicle.parking) {
            const parking = document.createElement("p");
            parking.textContent = `Parked at ${vehicle.parking.area_name} · ${vehicle.parking.slot_number}`;
            details.append(parking);
        }

        const actions = document.createElement("div");
        actions.className = "vehicle-actions";
        const actionItems = vehicle.parking
            ? [["checkout", "Check out"], ["edit", "Edit"], ["delete", "Delete"]]
            : [["slots", "Choose slot"], ["edit", "Edit"], ["delete", "Delete"]];
        for (const [action, label] of actionItems) {
            const button = document.createElement("button");
            button.type = "button";
            button.dataset.action = action;
            button.dataset.vehicleId = vehicle.vehicle_id;
            button.textContent = label;
            if (action === "edit" && vehicle.parking) {
                button.disabled = true;
                button.title = "Check out this vehicle before editing it.";
            }
            if (action === "delete") {
                button.disabled = vehicle.can_delete !== true || Boolean(vehicle.parking);
                button.title = vehicle.parking
                    ? "Check out this vehicle before deleting it."
                    : "Permanently delete this vehicle and its associated parking and reservation history.";
                if (vehicle.parking) {
                    const notice = document.createElement("p");
                    notice.className = "vehicle-action-note";
                    notice.textContent = "Check out this vehicle before deleting it.";
                    details.append(notice);
                }
            }
            actions.append(button);
        }
        item.append(details, actions);
        vehicleList.append(item);
    });
}

async function loadVehicles() {
    vehicleList.replaceChildren();
    const loading = document.createElement("p");
    loading.className = "muted";
    loading.textContent = "Loading your vehicles...";
    vehicleList.append(loading);
    try {
        const response = await fetch(`${API_BASE}/api/vehicles`, { headers: authHeaders() });
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.vehicles)) {
            throw new Error(data.error?.message || "Your vehicles could not be loaded.");
        }
        renderVehicles(data.vehicles);
        setVehicleMessage("");
    } catch (error) {
        vehicleList.replaceChildren();
        setVehicleMessage(error.message, true);
    }
}

vehicleForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    setVehicleMessage("");
    const vehicleId = vehicleForm.elements.vehicle_id.value;
    const values = {
        registration_number: vehicleForm.elements.registration_number.value,
        vehicle_type: vehicleForm.elements.vehicle_type.value,
        is_electric: vehicleForm.elements.is_electric.checked
    };
    try {
        const response = await fetch(
            `${API_BASE}/api/vehicles${vehicleId ? `/${encodeURIComponent(vehicleId)}` : ""}`,
            {
                method: vehicleId ? "PUT" : "POST",
                headers: { ...authHeaders(), "Content-Type": "application/json" },
                body: JSON.stringify(values)
            }
        );
        const data = response.status === 204 ? {} : await response.json();
        if (!response.ok) {
            throw new Error(data.error?.message || "The vehicle could not be saved.");
        }
        resetVehicleForm();
        await loadVehicles();
        setVehicleMessage(vehicleId ? "Vehicle updated." : "Vehicle added.");
        if (!vehicleId && data.vehicle?.vehicle_id) {
            await showAvailableSlots(data.vehicle.vehicle_id);
        }
    } catch (error) {
        setVehicleMessage(error.message, true);
    }
});

cancelVehicleEdit.addEventListener("click", resetVehicleForm);

vehicleList.addEventListener("click", async (event) => {
    const button = event.target.closest("button[data-action]");
    if (!button) {
        return;
    }
    const vehicleId = button.dataset.vehicleId;
    if (button.dataset.action === "slots") {
        await showAvailableSlots(vehicleId);
        return;
    }
    if (button.dataset.action === "checkout") {
        try {
            const response = await fetch(`${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}/checkout`, {
                method: "POST",
                headers: authHeaders()
            });
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error?.message || "The vehicle could not be checked out.");
            }
            await loadVehicles();
            setVehicleMessage(data.message || "Vehicle checked out.");
            const parkingArea = areas.find((area) => String(area.area_id) === String(data.area_id));
            if (parkingArea) {
                await loadFloor(parkingArea);
                const releasedSlot = parkingMap.querySelector(
                    `[data-slot-id="${CSS.escape(String(data.slot_id))}"]`
                );
                if (releasedSlot) {
                    selectSlot(releasedSlot);
                }
            }
        } catch (error) {
            setVehicleMessage(error.message, true);
        }
        return;
    }
    if (button.dataset.action === "edit") {
        if (button.disabled) {
            setVehicleMessage(button.title, true);
            return;
        }
        try {
            const response = await fetch(`${API_BASE}/api/vehicles`, { headers: authHeaders() });
            const data = await response.json();
            if (!response.ok || !Array.isArray(data.vehicles)) {
                throw new Error(data.error?.message || "Your vehicles could not be loaded.");
            }
            const vehicle = data.vehicles.find((item) => String(item.vehicle_id) === vehicleId);
            if (!vehicle) {
                throw new Error("That vehicle is no longer available.");
            }
            vehicleForm.elements.vehicle_id.value = vehicle.vehicle_id;
            vehicleForm.elements.registration_number.value = vehicle.registration_number;
            vehicleForm.elements.vehicle_type.value = vehicle.vehicle_type;
            vehicleForm.elements.is_electric.checked = vehicle.is_electric;
            saveVehicleButton.textContent = "Save changes";
            cancelVehicleEdit.hidden = false;
            vehicleForm.elements.registration_number.focus();
            setVehicleMessage(`Editing ${vehicle.registration_number}.`);
        } catch (error) {
            setVehicleMessage(error.message, true);
        }
        return;
    }

    if (!window.confirm("Permanently delete this vehicle and its associated parking, reservation, payment, receipt, and history records? This cannot be undone.")) {
        return;
    }
    try {
        const response = await fetch(`${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}`, {
            method: "DELETE",
            headers: authHeaders()
        });
        const data = response.status === 204 ? {} : await response.json();
        if (!response.ok) {
            throw new Error(data.error?.message || "The vehicle could not be deleted.");
        }
        setVehicleMessage("Vehicle deleted.");
        await loadVehicles();
    } catch (error) {
        setVehicleMessage(error.message, true);
    }
});

async function showAvailableSlots(vehicleId) {
    const card = vehicleList.querySelector(`[data-vehicle-id="${CSS.escape(String(vehicleId))}"]`);
    if (!card) {
        return;
    }
    const existing = card.querySelector(".available-slot-options");
    if (existing) {
        existing.remove();
        if (slotSelection?.vehicleId === String(vehicleId)) {
            slotSelection = null;
            applySlotSelection();
        }
        return;
    }
    setVehicleMessage("Loading available compatible slots...");
    try {
        const response = await fetch(
            `${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}/available-slots`,
            { headers: authHeaders() }
        );
        const data = await response.json();
        if (!response.ok || !Array.isArray(data.slots)) {
            throw new Error(data.error?.message || "Available slots could not be loaded.");
        }

        const options = document.createElement("div");
        options.className = "available-slot-options";
        const heading = document.createElement("strong");
        heading.textContent = "Choose an available slot";
        options.append(heading);
        if (data.slots.length === 0) {
            slotSelection = null;
            applySlotSelection();
            const empty = document.createElement("p");
            empty.className = "muted";
            empty.textContent = "No compatible slots are currently available.";
            options.append(empty);
        } else {
            slotSelection = {
                vehicleId: String(vehicleId),
                slotIds: new Set(data.slots.map((slot) => String(slot.slot_id)))
            };
            applySlotSelection();
            data.slots.forEach((slot) => {
                const choice = document.createElement("button");
                choice.type = "button";
                choice.className = "slot-choice";
                choice.dataset.slotId = slot.slot_id;
                choice.dataset.vehicleId = vehicleId;
                choice.textContent = `${slot.area_name} · ${slot.slot_number}${slot.is_ev ? " · EV charging" : ""}`;
                options.append(choice);
            });
        }
        card.append(options);
        setVehicleMessage("");
    } catch (error) {
        setVehicleMessage(error.message, true);
    }
}

function applySlotSelection() {
    parkingMap.querySelectorAll(".slot").forEach((slot) => {
        const selectable = slotSelection &&
            slotSelection.slotIds.has(String(slot.dataset.slotId));
        if (selectable) {
            slot.dataset.selectVehicleId = slotSelection.vehicleId;
            slot.classList.add("slot-selectable");
            slot.title = "Choose this available slot";
        } else {
            delete slot.dataset.selectVehicleId;
            slot.classList.remove("slot-selectable");
        }
    });
}

async function parkVehicleInSlot(vehicleId, slotId) {
    const selectedElement = parkingMap.querySelector(
        `[data-slot-id="${CSS.escape(String(slotId))}"]`
    );
    if (selectedElement) {
        selectedElement.setAttribute("aria-disabled", "true");
    }
    try {
        const response = await fetch(
            `${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}/park`,
            {
                method: "POST",
                headers: { ...authHeaders(), "Content-Type": "application/json" },
                body: JSON.stringify({ slot_id: Number(slotId) })
            }
        );
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.error?.message || "The vehicle could not be parked in this slot.");
        }
        slotSelection = null;
        await loadVehicles();
        setVehicleMessage(`Vehicle parked at ${data.parking.area_name} · ${data.parking.slot_number}.`);
        const parkingArea = areas.find((area) => String(area.area_id) === String(data.parking.area_id));
        if (parkingArea) {
            selectedArea = parkingArea;
            floorTabs.forEach((tab) => {
                tab.classList.toggle("active", String(tab.dataset.areaId) === String(parkingArea.area_id));
            });
            await loadFloor(parkingArea);
            const parkedSlot = parkingMap.querySelector(
                `[data-slot-id="${CSS.escape(String(data.parking.slot_id))}"]`
            );
            if (parkedSlot) {
                selectSlot(parkedSlot);
            }
        }
    } catch (error) {
        if (selectedElement) {
            selectedElement.removeAttribute("aria-disabled");
        }
        setVehicleMessage(error.message, true);
    }
}

vehicleList.addEventListener("click", async (event) => {
    const choice = event.target.closest("button.slot-choice");
    if (!choice) {
        return;
    }
    await parkVehicleInSlot(choice.dataset.vehicleId, choice.dataset.slotId);
});

restoreSession();
