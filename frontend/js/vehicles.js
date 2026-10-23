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
            showVehicleExitModal(vehicleId);
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
async function showVehicleExitModal(vehicleId) {
    const sessionExitDialog = document.getElementById("sessionExitDialog");
    const exitModalTitle = document.getElementById("exitModalTitle");
    const exitModalBody = document.getElementById("exitModalBody");

    if (!sessionExitDialog || !exitModalBody) return;

    if (exitModalTitle) {
        exitModalTitle.textContent = "Vehicle Check Out & Fee Summary";
    }

    exitModalBody.replaceChildren();
    const loading = document.createElement("p");
    loading.className = "muted";
    loading.textContent = "Calculating current duration and fees...";
    exitModalBody.append(loading);

    sessionExitDialog.showModal();

    try {
        const response = await fetch(`${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}/fee-summary`, {
            headers: authHeaders()
        });
        const data = await response.json();
        if (!response.ok || !data.session) {
            throw new Error(data.error?.message || "Failed to load fee summary for this vehicle.");
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

        if (summary.reserved_end_time) {
            const reservedRow = document.createElement("div");
            reservedRow.className = "fee-breakdown-row";
            reservedRow.innerHTML = `<span class="muted">Reserved Until</span><span>${formatIsoTimestamp(summary.reserved_end_time)}</span>`;
            card.append(reservedRow);
        }

        const durRow = document.createElement("div");
        durRow.className = "fee-breakdown-row";
        durRow.innerHTML = `<span class="muted">Parking Duration</span><span>${summary.duration_minutes} min (billed as ${summary.billed_hours} hr)</span>`;
        card.append(durRow);

        const baseFeeRow = document.createElement("div");
        baseFeeRow.className = "fee-breakdown-row";
        baseFeeRow.innerHTML = `<span>Base Parking Fee (${summary.billed_hours}h × ₹${Number(summary.hourly_rate).toFixed(2)})</span><span>₹${Number(summary.base_fee).toFixed(2)}</span>`;
        card.append(baseFeeRow);

        if (summary.overstay_duration_minutes > 0) {
            const overstayWarning = document.createElement("div");
            overstayWarning.className = "fee-overstay-warning";
            overstayWarning.textContent = `⚠ Overstayed reserved time by ${summary.overstay_duration_minutes} min (billed as ${summary.billed_overstay_hours} hr @ ₹${Number(summary.overstay_rate).toFixed(2)}/hr)`;
            card.append(overstayWarning);

            const overstayFeeRow = document.createElement("div");
            overstayFeeRow.className = "fee-breakdown-row";
            overstayFeeRow.innerHTML = `<span>Overstay Charge</span><span>₹${Number(summary.overstay_fee).toFixed(2)}</span>`;
            card.append(overstayFeeRow);
        }

        const totalRow = document.createElement("div");
        totalRow.className = "fee-breakdown-row total";
        totalRow.innerHTML = `<span>Total Fee to Pay</span><span>₹${Number(summary.total_fee).toFixed(2)}</span>`;
        card.append(totalRow);

        exitModalBody.append(card);

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
                const checkoutRes = await fetch(`${API_BASE}/api/vehicles/${encodeURIComponent(vehicleId)}/checkout`, {
                    method: "POST",
                    headers: {
                        ...authHeaders(),
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({
                        payment_action: select.value
                    })
                });
                const checkoutData = await checkoutRes.json();
                if (!checkoutRes.ok) {
                    throw new Error(checkoutData.error?.message || "Failed to check out vehicle.");
                }

                sessionExitDialog.close();
                const finalFee = checkoutData.session ? ` Total fee: ₹${Number(checkoutData.session.total_fee).toFixed(2)}.` : "";
                setVehicleMessage(`Vehicle ${summary.registration_number} checked out successfully! Slot ${summary.slot_number} released.${finalFee}`);

                await loadVehicles();
                if (typeof loadActiveParkingSessions === "function") {
                    await loadActiveParkingSessions();
                }
                if (typeof loadReservations === "function") {
                    await loadReservations();
                }
                const parkingArea = areas.find((area) => String(area.area_id) === String(checkoutData.area_id));
                if (parkingArea) {
                    await loadFloor(parkingArea);
                    const releasedSlot = parkingMap.querySelector(
                        `[data-slot-id="${CSS.escape(String(checkoutData.slot_id))}"]`
                    );
                    if (releasedSlot) {
                        selectSlot(releasedSlot);
                    }
                }
            } catch (err) {
                confirmBtn.disabled = false;
                confirmBtn.textContent = "Confirm Exit & Release Slot";
                alert(err.message);
            }
        });

        actionSec.append(cancelBtn, confirmBtn);
        exitModalBody.append(actionSec);
    } catch (err) {
        exitModalBody.replaceChildren();
        const errP = document.createElement("p");
        errP.className = "muted";
        errP.textContent = err.message;
        exitModalBody.append(errP);
    }
}
