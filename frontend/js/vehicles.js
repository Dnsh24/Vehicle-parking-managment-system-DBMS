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
    if (vehicleMessage) {
        vehicleMessage.textContent = message;
        vehicleMessage.classList.toggle("is-error", isError);
    }
    if (message && typeof showToast === "function") {
        showToast(message, isError ? "error" : "success");
    }
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
            let text = `Parked at ${vehicle.parking.area_name} · ${vehicle.parking.slot_number}`;
            if (vehicle.parking.planned_end_time) {
                text += ` (Planned until: ${formatIsoTimestamp(vehicle.parking.planned_end_time)})`;
            }
            parking.textContent = text;
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
            const actionText = vehicleId ? "Vehicle Updated" : "Vehicle Added Successfully! 🚗";
            setVehicleMessage(vehicleId ? "Vehicle updated." : "Vehicle added.");
            const v = data.vehicle;
            if (v) {
                await showAppNotice({
                    title: actionText,
                    message: `${v.registration_number} has been registered and is ready for parking or reservations.`,
                    type: "success",
                    details: [
                        { label: "Registration No.", value: v.registration_number },
                        { label: "Vehicle Type", value: v.vehicle_type.toUpperCase() },
                        { label: "Electric Vehicle (EV)", value: v.is_electric ? "Yes" : "No" },
                        { label: "Accessible Parking", value: v.needs_accessible_parking ? "Yes" : "No" }
                    ],
                    confirmText: "Got It"
                });
            }
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
            await showAppNotice({
                title: "Unable to Save Vehicle",
                message: error.message,
                type: "danger",
                confirmText: "Close"
            });
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

        const confirmed = await showAppConfirm({
            title: "Permanently Delete Vehicle?",
            message: "Permanently delete this vehicle and its associated parking, reservation, payment, receipt, and history records? This cannot be undone.",
            type: "danger",
            confirmText: "Permanently Delete",
            cancelText: "Cancel"
        });
        if (!confirmed) {
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
            await showAppNotice({
                title: "Vehicle Deleted",
                message: "The vehicle and its associated history records have been permanently removed.",
                type: "info",
                confirmText: "OK"
            });
            await loadVehicles();
        } catch (error) {
            setVehicleMessage(error.message, true);
            await showAppNotice({
                title: "Unable to Delete Vehicle",
                message: error.message,
                type: "danger",
                confirmText: "Close"
            });
        }
    });

    vehicleList.addEventListener("click", async (event) => {
        const choice = event.target.closest("button.slot-choice");
        if (!choice) {
            return;
        }
        await promptImmediateParking(
            choice.dataset.vehicleId,
            choice.dataset.slotId,
            choice.textContent,
            choice.dataset.reservationStart || ""
        );
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
        if (data.reservation) {
            const notice = document.createElement("p");
            notice.className = "muted";
            notice.textContent = `Reservation #${data.reservation.reservation_id} is ${data.reservation.status} for ${data.reservation.area_name}, slot ${data.reservation.slot_number}, from ${formatIsoTimestamp(data.reservation.start_time)} to ${formatIsoTimestamp(data.reservation.end_time)}. Slots reserved for that booking are hidden. Immediate parking duration must finish before this reservation starts; otherwise use Reservations to check in or cancel it.`;
            options.append(notice);
        }
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
                choice.dataset.reservationStart = data.reservation?.start_time || "";
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

function promptImmediateParking(vehicleId, slotId, slotLabel, reservationStartIso = "") {
    const reservationStartMs = Date.parse(reservationStartIso);
    const maxMinutesBeforeReservation = Number.isFinite(reservationStartMs)
        ? Math.floor((reservationStartMs - Date.now()) / 60000) - 1
        : null;
    if (maxMinutesBeforeReservation !== null && maxMinutesBeforeReservation < 1) {
        showAppNotice({
            title: "Reservation Check-In Due",
            message: "This vehicle’s reservation has started or is less than one minute away. Check in or cancel the reservation from the Reservations section before parking it elsewhere.",
            type: "info",
            confirmText: "Close"
        });
        return Promise.resolve(false);
    }

    const dialog = document.getElementById("immediateParkingDialog");
    if (!dialog) {
        const fallbackDuration = maxMinutesBeforeReservation === null
            ? 120
            : Math.min(120, maxMinutesBeforeReservation);
        return parkVehicleInSlot(vehicleId, slotId, fallbackDuration);
    }
    const titleEl = document.getElementById("immediateParkingTitle");
    const promptEl = document.getElementById("immediateParkingPrompt");
    const durationSelect = document.getElementById("immediateParkingDuration");
    const customGroup = document.getElementById("immediateCustomDurationGroup");
    const customHoursInput = document.getElementById("immediateCustomHours");
    const customMinutesInput = document.getElementById("immediateCustomMinutes");
    const closeBtn = document.getElementById("closeImmediateParkingDialog");
    const cancelBtn = document.getElementById("cancelImmediateParkingButton");
    const form = document.getElementById("immediateParkingForm");

    if (titleEl && slotLabel) {
        titleEl.textContent = `Park Vehicle · ${slotLabel.trim()}`;
    }
    if (promptEl) {
        promptEl.textContent = maxMinutesBeforeReservation === null
            ? "How long do you plan to stay? (Durations under 1 hour are charged the 1-hour slot rate)"
            : `Reservation starts at ${formatIsoTimestamp(reservationStartIso)}. Choose a stay of at most ${maxMinutesBeforeReservation} minutes so this vehicle is free for check-in.`;
    }
    if (durationSelect) {
        const presetOptions = Array.from(durationSelect.options).filter((option) => /^\d+$/.test(option.value));
        presetOptions.forEach((option) => {
            option.disabled = maxMinutesBeforeReservation !== null && Number(option.value) > maxMinutesBeforeReservation;
        });
        const defaultPreset = presetOptions.find((option) => !option.disabled && option.value === "120") ||
            presetOptions.filter((option) => !option.disabled).pop();
        durationSelect.value = defaultPreset ? defaultPreset.value : "custom";
    }
    if (customGroup) {
        customGroup.style.display = maxMinutesBeforeReservation !== null && maxMinutesBeforeReservation < 120
            ? "block"
            : "none";
    }
    if (customHoursInput) {
        customHoursInput.value = "0";
    }
    if (customMinutesInput) {
        customMinutesInput.max = maxMinutesBeforeReservation === null
            ? "59"
            : String(Math.min(59, maxMinutesBeforeReservation));
        customMinutesInput.value = maxMinutesBeforeReservation === null
            ? "30"
            : String(Math.min(30, maxMinutesBeforeReservation));
    }

    const onDurationChange = () => {
        if (customGroup) {
            customGroup.style.display = durationSelect.value === "custom" ? "block" : "none";
        }
    };
    durationSelect?.removeEventListener("change", onDurationChange);
    durationSelect?.addEventListener("change", onDurationChange);

    return new Promise((resolve) => {
        const cleanup = () => {
            dialog.close();
            form?.removeEventListener("submit", onSubmit);
            closeBtn?.removeEventListener("click", onCancel);
            cancelBtn?.removeEventListener("click", onCancel);
        };
        const onCancel = () => {
            cleanup();
            resolve(false);
        };
        const onSubmit = async (e) => {
            e.preventDefault();
            let totalMinutes = 120;
            if (durationSelect && durationSelect.value === "custom") {
                const hours = Math.max(0, Math.min(24, parseInt(customHoursInput?.value || "0", 10) || 0));
                const mins = Math.max(0, Math.min(59, parseInt(customMinutesInput?.value || "0", 10) || 0));
                totalMinutes = Math.max(1, Math.min(1440, (hours * 60) + mins));
            } else if (durationSelect) {
                totalMinutes = Math.max(1, Math.min(1440, parseInt(durationSelect.value, 10) || 120));
            }
            if (maxMinutesBeforeReservation !== null && totalMinutes > maxMinutesBeforeReservation) {
                if (durationSelect) durationSelect.value = "custom";
                if (customGroup) customGroup.style.display = "block";
                if (promptEl) {
                    promptEl.textContent = `This reservation starts at ${formatIsoTimestamp(reservationStartIso)}. Choose a stay of at most ${maxMinutesBeforeReservation} minutes.`;
                }
                return;
            }
            cleanup();
            await parkVehicleInSlot(vehicleId, slotId, totalMinutes);
            resolve(true);
        };

        form?.removeEventListener("submit", onSubmit);
        form?.addEventListener("submit", onSubmit, { once: true });
        closeBtn?.removeEventListener("click", onCancel);
        closeBtn?.addEventListener("click", onCancel, { once: true });
        cancelBtn?.removeEventListener("click", onCancel);
        cancelBtn?.addEventListener("click", onCancel, { once: true });

        dialog.showModal();
    });
}

async function parkVehicleInSlot(vehicleId, slotId, durationMinutes = 120) {
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
                body: JSON.stringify({
                    slot_id: Number(slotId),
                    duration_minutes: Number(durationMinutes)
                })
            }
        );
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.error?.message || "The vehicle could not be parked in this slot.");
        }
        slotSelection = null;
        await loadVehicles();
        setVehicleMessage(`Vehicle parked at ${data.parking.area_name} · ${data.parking.slot_number}.`);
        await showAppNotice({
            title: "Vehicle Parked Successfully! 🚗",
            message: `${data.parking.registration_number} is now checked in and parked at ${data.parking.area_name}, Slot ${data.parking.slot_number}.`,
            type: "success",
            subtitle: `Active Session #${data.parking.session_id}`,
            details: [
                { label: "Vehicle", value: data.parking.registration_number },
                { label: "Parking Slot", value: `${data.parking.area_name} · Slot ${data.parking.slot_number}` },
                { label: "Planned Duration", value: `${durationMinutes} minute(s)` },
                { label: "Planned Exit Time", value: formatIsoTimestamp(data.parking.planned_end_time) },
                { label: "Grace Period", value: "7 min free after planned exit, then ₹1/min" }
            ],
            confirmText: "View Parking Session"
        });
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
        await showAppNotice({
            title: "Unable to Park Vehicle",
            message: error.message,
            type: "danger",
            confirmText: "Close"
        });
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

        const endTarget = summary.planned_end_time || summary.reserved_end_time;
        if (endTarget) {
            const reservedRow = document.createElement("div");
            reservedRow.className = "fee-breakdown-row";
            reservedRow.innerHTML = `<span class="muted">Planned / Reserved Until</span><span>${formatIsoTimestamp(endTarget)}</span>`;
            card.append(reservedRow);
        }

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
                overstayWarning.textContent = `⚠ Overstayed planned time by ${overstayMins} min (7 min free grace + ${billedOvertimeMins} billable min @ ₹${overtimeRate.toFixed(2)}/min)`;
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
                overstayWarning.textContent = `ℹ Overstayed planned time by ${overstayMins} min (within 7-minute free grace period — ₹0 overtime)`;
                card.append(overstayWarning);
            }
        }

        const totalRow = document.createElement("div");
        totalRow.className = "fee-breakdown-row total";
        totalRow.innerHTML = `<span>Total Fee to Pay</span><span>₹${Number(summary.total_fee || 0).toFixed(2)}</span>`;
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

                const sess = checkoutData.session;
                await showAppNotice({
                    title: "Check Out Complete! 🏁",
                    message: `Vehicle ${summary.registration_number} checked out successfully! Slot ${summary.slot_number} released.`,
                    type: "success",
                    subtitle: sess ? `Session #${sess.session_id}` : "",
                    details: [
                        { label: "Vehicle", value: summary.registration_number },
                        { label: "Slot Released", value: summary.slot_number },
                        { label: "Total Billed Fee", value: sess ? `₹${Number(sess.total_fee).toFixed(2)}` : "N/A" },
                        { label: "Payment Status", value: sess ? (sess.payment_status || "paid").replace(/_/g, " ").toUpperCase() : "PAID" }
                    ],
                    confirmText: "Done"
                });

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
                await showAppNotice({
                    title: "Check Out Failed",
                    message: err.message,
                    type: "danger"
                });
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
