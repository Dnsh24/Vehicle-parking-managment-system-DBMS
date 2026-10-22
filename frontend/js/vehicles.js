// =====================================================================
// ParkFlow - Vehicle Management Module
// =====================================================================

const vehicleForm = document.getElementById("vehicleForm");
const vehicleList = document.getElementById("vehicleList");
const vehicleMessage = document.getElementById("vehicleMessage");
const saveVehicleButton = document.getElementById("saveVehicleButton");
const cancelVehicleEdit = document.getElementById("cancelVehicleEdit");
const vehicleAfterAddField = document.getElementById("vehicleAfterAddField");

function setVehicleMessage(message, isError = false) {
    if (!vehicleMessage) return;
    vehicleMessage.textContent = message;
    vehicleMessage.classList.toggle("is-error", isError);
}

function resetVehicleForm() {
    if (!vehicleForm) return;
    vehicleForm.reset();
    vehicleForm.elements.vehicle_id.value = "";
    if (saveVehicleButton) saveVehicleButton.textContent = "Add vehicle";
    if (cancelVehicleEdit) cancelVehicleEdit.hidden = true;
    if (vehicleAfterAddField) vehicleAfterAddField.hidden = false;
}

function renderVehicles(vehicles) {
    if (!vehicleList) return;
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
        description.textContent = `${type}${vehicle.is_electric ? " · Electric" : ""}${vehicle.needs_accessible_parking ? " · Handicapped parking required" : ""}`;
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
    if (!vehicleList) return;
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
        if (typeof updateReservationVehicleOptions === "function") {
            updateReservationVehicleOptions(data.vehicles);
        }
    } catch (error) {
        vehicleList.replaceChildren();
        setVehicleMessage(error.message, true);
    }
}

if (vehicleForm) {
    vehicleForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        setVehicleMessage("");
        const vehicleId = vehicleForm.elements.vehicle_id.value;
        const afterAddAction = vehicleForm.elements.after_add_action.value;
        const values = {
            registration_number: vehicleForm.elements.registration_number.value,
            vehicle_type: vehicleForm.elements.vehicle_type.value,
            is_electric: vehicleForm.elements.is_electric.checked,
            needs_accessible_parking: vehicleForm.elements.needs_accessible_parking.checked
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
                if (afterAddAction === "park") {
                    await showAvailableSlots(data.vehicle.vehicle_id);
                } else {
                    if (reservationVehicle) {
                        reservationVehicle.value = String(data.vehicle.vehicle_id);
                        reservationVehicle.dispatchEvent(new Event("change"));
                    }
                    if (reservationSearchForm) {
                        reservationSearchForm.scrollIntoView({ behavior: "smooth", block: "center" });
                        reservationSearchForm.elements.start_time.focus({ preventScroll: true });
                    }
                    setVehicleMessage("Vehicle added. Choose a reservation time and search for a slot.");
                }
            }
        } catch (error) {
            setVehicleMessage(error.message, true);
        }
    });
}

if (cancelVehicleEdit) {
    cancelVehicleEdit.addEventListener("click", resetVehicleForm);
}

if (vehicleList) {
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
                vehicleForm.elements.needs_accessible_parking.checked = vehicle.needs_accessible_parking;
                if (saveVehicleButton) saveVehicleButton.textContent = "Save changes";
                if (cancelVehicleEdit) cancelVehicleEdit.hidden = false;
                if (vehicleAfterAddField) vehicleAfterAddField.hidden = true;
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

    vehicleList.addEventListener("click", async (event) => {
        const choice = event.target.closest("button.slot-choice");
        if (!choice) {
            return;
        }
        await parkVehicleInSlot(choice.dataset.vehicleId, choice.dataset.slotId);
    });
}

async function showAvailableSlots(vehicleId) {
    if (!vehicleList) return;
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
                const features = [
                    slot.is_ev || /\bev\b|electric|charg/i.test(slot.slot_type) ? "EV charging" : "",
                    /handicap|accessible/i.test(slot.slot_type) ? "Handicapped" : ""
                ].filter(Boolean);
                choice.textContent = `${slot.area_name} · ${slot.slot_number}${features.length ? ` · ${features.join(" · ")}` : ""}`;
                options.append(choice);
            });
        }
        card.append(options);
        setVehicleMessage("");
    } catch (error) {
        setVehicleMessage(error.message, true);
    }
}

async function parkVehicleInSlot(vehicleId, slotId) {
    if (!parkingMap) return;
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
