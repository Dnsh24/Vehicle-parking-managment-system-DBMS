// =====================================================================
// ParkFlow - Parking Map & Area Management Module
// =====================================================================

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
const refreshButton = document.getElementById("refreshButton");
const changeAreaButton = document.getElementById("changeAreaButton");

let areas = [];
let selectedArea = null;
let selectedParkedSeconds = 0;
let selectedDurationTimer = null;
let slotSelection = null;

function renderMessage(title, message, action = "") {
    if (parkingMap) {
        parkingMap.innerHTML = `<div class="map-message"><strong>${title}</strong><span>${message}</span>${action}</div>`;
    }
}

function renderSelectedVisual(slot, statusClass, vehicleType) {
    if (!selectedVisual) return;
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
    if (selectedSlot) selectedSlot.textContent = slot.dataset.slot;
    if (selectedStatus) {
        selectedStatus.textContent = status;
        selectedStatus.className = `status-pill ${statusClass}`;
    }
    renderSelectedVisual(slot, statusClass, slot.dataset.vehicle);
    if (detailStatus) detailStatus.textContent = status === "Available" ? "Available to use" : status;
    const actualVehicleType = slot.dataset.vehicleRegistration
        ? slot.dataset.vehicle
        : null;
    if (detailVehicle) {
        detailVehicle.textContent = actualVehicleType
            ? displayVehicleType(actualVehicleType, slot.dataset.vehicleElectric === "true")
            : vehicle;
    }
    if (detailType) detailType.textContent = type;
    const registration = slot.dataset.vehicleRegistration || "";
    if (detailRegistrationRow) detailRegistrationRow.hidden = !registration;
    if (detailRegistration) detailRegistration.textContent = registration;
    if (detailDurationRow) detailDurationRow.hidden = !registration || !slot.dataset.parkedAt;
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
            if (detailDuration) {
                detailDuration.textContent = formatDuration(Math.max(selectedParkedSeconds, liveSeconds));
            }
        };
        updateDuration();
        selectedDurationTimer = window.setInterval(updateDuration, 30000);
    }
    if (detailFeatures) {
        detailFeatures.innerHTML = slot.classList.contains("ev")
            ? '<span class="feature-tag">⚡ EV charging</span>'
            : slot.classList.contains("accessible")
                ? '<span class="feature-tag">♿ Accessible</span>'
                : '<span class="muted">Standard</span>';
    }
}

function applySlotSelection() {
    if (!parkingMap) return;
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

function renderFloor(area, slots) {
    if (!parkingMap) return;
    const groupedSlots = {
        truck: [],
        bike: [],
        car: [],
        "ev-car": [],
        "accessible-car": []
    };

    slots.forEach((slot) => {
        const section = sectionForSlot(slot);
        if (groupedSlots[section]) {
            groupedSlots[section].push(slot);
        }
    });

    const sections = [
        ["truck", "Truck / oversized"],
        ["bike", "Bike & scooter parking"],
        ["car", "Normal car parking"],
        ["ev-car", "EV charging"],
        ["accessible-car", "Handicapped parking"]
    ];

    if (floorTitle) floorTitle.textContent = area.name;
    if (areaName) areaName.textContent = area.name;
    const availableSlots = slots.filter((slot) => slot.status === "available").length;
    if (availableCount) {
        availableCount.innerHTML = `${availableSlots} <small>of ${area.capacity} slots</small>`;
    }
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
            const regEl = element.querySelector(".slot-registration");
            if (regEl) regEl.textContent = slot.vehicle_registration;
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
                if (typeof parkVehicleInSlot === "function") {
                    parkVehicleInSlot(slot.dataset.selectVehicleId, slot.dataset.slotId);
                }
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
                    if (typeof parkVehicleInSlot === "function") {
                        parkVehicleInSlot(slot.dataset.selectVehicleId, slot.dataset.slotId);
                    }
                    return;
                }
                selectSlot(slot);
            }
        });
    });
}

async function loadFloor(area) {
    selectedArea = area;
    if (floorTitle) floorTitle.textContent = `Loading ${area.name}`;
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
        const retryBtn = parkingMap ? parkingMap.querySelector(".map-retry") : null;
        if (retryBtn) {
            retryBtn.addEventListener("click", () => loadFloor(area));
        }
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
        const retryBtn = parkingMap ? parkingMap.querySelector(".map-retry") : null;
        if (retryBtn) {
            retryBtn.addEventListener("click", loadAreas);
        }
    }
}

if (refreshButton) {
    refreshButton.addEventListener("click", (event) => {
        const button = event.currentTarget;
        button.innerHTML = '<span aria-hidden="true">↻</span> Updated';
        if (selectedArea) {
            loadFloor(selectedArea);
        }
        window.setTimeout(() => {
            button.innerHTML = '<span aria-hidden="true">↻</span> Refresh';
        }, 1200);
    });
}

if (changeAreaButton) {
    changeAreaButton.addEventListener("click", () => {
        showAppNotice({
            title: "Floor & Area Selection",
            message: "Mall Central floors are available in the floor selector tab bar above the parking layout.",
            type: "info"
        });
    });
}
