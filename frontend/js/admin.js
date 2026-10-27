// =====================================================================
// ParkFlow - Phase 4: Administrator Management Console Module
// =====================================================================

const adminBadge = document.getElementById("adminBadge");
const viewSwitcher = document.getElementById("viewSwitcher");
const viewUserMode = document.getElementById("viewUserMode");
const viewAdminMode = document.getElementById("viewAdminMode");
const adminConsole = document.getElementById("adminConsole");
const adminRefreshButton = document.getElementById("adminRefreshButton");
const adminGlobalMessage = document.getElementById("adminGlobalMessage");

const adminNavTabs = document.querySelectorAll(".admin-nav-tab");
const adminPanels = {
    users: document.getElementById("adminUsersPanel"),
    areas: document.getElementById("adminAreasPanel"),
    slots: document.getElementById("adminSlotsPanel"),
    rates: document.getElementById("adminRatesPanel"),
    maintenance: document.getElementById("adminMaintenancePanel")
};

// Users elements
const adminUsersMessage = document.getElementById("adminUsersMessage");
const adminUsersList = document.getElementById("adminUsersList");

// Areas elements
const adminAreasMessage = document.getElementById("adminAreasMessage");
const adminAreaForm = document.getElementById("adminAreaForm");
const adminAreasListEl = document.getElementById("adminAreasList");
const cancelAreaEdit = document.getElementById("cancelAreaEdit");
const saveAreaButton = document.getElementById("saveAreaButton");

// Slots elements
const adminSlotsMessage = document.getElementById("adminSlotsMessage");
const adminSlotAreaSelect = document.getElementById("adminSlotAreaSelect");
const adminSlotForm = document.getElementById("adminSlotForm");
const adminSlotsListEl = document.getElementById("adminSlotsList");
const cancelSlotEdit = document.getElementById("cancelSlotEdit");
const saveSlotButton = document.getElementById("saveSlotButton");

// Rates elements
const adminRatesMessage = document.getElementById("adminRatesMessage");
const adminRateAreaSelect = document.getElementById("adminRateAreaSelect");
const adminRateForm = document.getElementById("adminRateForm");
const adminRatesListEl = document.getElementById("adminRatesList");
const cancelRateEdit = document.getElementById("cancelRateEdit");
const saveRateButton = document.getElementById("saveRateButton");

// Maintenance elements
const adminMaintenanceMessage = document.getElementById("adminMaintenanceMessage");
const adminMaintenanceSlotSelect = document.getElementById("adminMaintenanceSlotSelect");
const adminMaintenanceForm = document.getElementById("adminMaintenanceForm");
const adminMaintenanceListEl = document.getElementById("adminMaintenanceList");
const cancelMaintenanceEdit = document.getElementById("cancelMaintenanceEdit");
const saveMaintenanceButton = document.getElementById("saveMaintenanceButton");

let currentAdminTab = "users";
let adminAreasCache = [];
let adminSlotsCache = [];
let adminRatesCache = [];
let adminMaintenanceCache = [];

function setAdminMessage(element, message, isError = false) {
    if (!element) return;
    element.textContent = message;
    element.classList.toggle("is-error", isError);
    if (message && typeof showToast === "function") {
        showToast(message, isError ? "error" : "success");
    }
    if (message) {
        setTimeout(() => {
            if (element.textContent === message) {
                element.textContent = "";
                element.classList.remove("is-error");
            }
        }, 5000);
    }
}

function switchViewMode(mode) {
    const protectedContent = document.getElementById("protectedContent");
    if (mode === "admin") {
        if (protectedContent) protectedContent.hidden = true;
        if (adminConsole) adminConsole.hidden = false;
        if (viewAdminMode) viewAdminMode.classList.add("active");
        if (viewUserMode) viewUserMode.classList.remove("active");
        loadAdminTab(currentAdminTab);
    } else {
        if (adminConsole) adminConsole.hidden = true;
        if (protectedContent) protectedContent.hidden = false;
        if (viewUserMode) viewUserMode.classList.add("active");
        if (viewAdminMode) viewAdminMode.classList.remove("active");
    }
}

if (viewUserMode && viewAdminMode) {
    viewUserMode.addEventListener("click", () => switchViewMode("user"));
    viewAdminMode.addEventListener("click", () => switchViewMode("admin"));
}

adminNavTabs.forEach((tab) => {
    tab.addEventListener("click", () => {
        const target = tab.dataset.adminTab;
        currentAdminTab = target;
        adminNavTabs.forEach((t) => t.classList.toggle("active", t === tab));
        Object.entries(adminPanels).forEach(([key, panel]) => {
            if (panel) panel.hidden = key !== target;
        });
        loadAdminTab(target);
    });
});

if (adminRefreshButton) {
    adminRefreshButton.addEventListener("click", () => loadAdminTab(currentAdminTab));
}

function loadAdminTab(tab) {
    switch (tab) {
        case "users":
            loadAdminUsers();
            break;
        case "areas":
            loadAdminAreas();
            break;
        case "slots":
            loadAdminSlotsTab();
            break;
        case "rates":
            loadAdminRatesTab();
            break;
        case "maintenance":
            loadAdminMaintenanceTab();
            break;
    }
}

// ---------------------------------------------------------------------
// 1. Users Management
// ---------------------------------------------------------------------
async function loadAdminUsers() {
    if (!adminUsersList) return;
    adminUsersList.innerHTML = `<tr><td colspan="8" class="muted">Loading users...</td></tr>`;
    try {
        const data = await adminApiCall("/api/admin/users");
        if (!data.users || data.users.length === 0) {
            adminUsersList.innerHTML = `<tr><td colspan="8" class="muted">No registered users found.</td></tr>`;
            return;
        }
        adminUsersList.innerHTML = data.users.map((u) => {
            const isSelf = currentUser && currentUser.user_id === u.user_id;
            return `
                <tr>
                    <td><strong>#${u.user_id}</strong></td>
                    <td>${escapeHtml(u.username)}</td>
                    <td>${escapeHtml(u.name)}</td>
                    <td>${escapeHtml(u.email)}</td>
                    <td>${escapeHtml(u.phone || "—")}</td>
                    <td>
                        <span class="badge badge-${escapeHtml(u.role)}">${escapeHtml(u.role)}</span>
                    </td>
                    <td>
                        <span class="badge badge-${u.is_active ? 'active' : 'inactive'}">
                            ${u.is_active ? 'Active' : 'Inactive'}
                        </span>
                    </td>
                    <td>
                        <div class="admin-actions-cell">
                            ${isSelf ? '<span class="muted">(Current Account)</span>' : `
                                <select class="admin-user-role-select" data-user-id="${u.user_id}" aria-label="Change role for ${escapeHtml(u.username)}">
                                    <option value="user" ${u.role === 'user' ? 'selected' : ''}>User</option>
                                    <option value="admin" ${u.role === 'admin' ? 'selected' : ''}>Admin</option>
                                </select>
                                <button type="button" class="${u.is_active ? 'danger' : 'success'} admin-user-status-btn"
                                    data-user-id="${u.user_id}" data-active="${u.is_active}">
                                    ${u.is_active ? 'Deactivate' : 'Activate'}
                                </button>
                                <button type="button" class="danger admin-user-delete-btn"
                                    data-user-id="${u.user_id}" data-username="${escapeHtml(u.username)}">
                                    Delete
                                </button>
                            `}
                        </div>
                    </td>
                </tr>
            `;
        }).join("");
    } catch (error) {
        setAdminMessage(adminUsersMessage, error.message, true);
        adminUsersList.innerHTML = `<tr><td colspan="8" class="muted is-error">Failed to load users: ${escapeHtml(error.message)}</td></tr>`;
    }
}

if (adminUsersList) {
    adminUsersList.addEventListener("change", async (event) => {
        const select = event.target.closest(".admin-user-role-select");
        if (!select) return;
        const userId = select.dataset.userId;
        const newRole = select.value;
        try {
            await adminApiCall(`/api/admin/users/${encodeURIComponent(userId)}/role`, "PUT", { role: newRole });
            setAdminMessage(adminUsersMessage, `Updated role for user #${userId} to ${newRole}.`);
            await loadAdminUsers();
        } catch (error) {
            setAdminMessage(adminUsersMessage, error.message, true);
            await loadAdminUsers();
        }
    });

    adminUsersList.addEventListener("click", async (event) => {
        const delBtn = event.target.closest(".admin-user-delete-btn");
        if (delBtn) {
            const userId = delBtn.dataset.userId;
            const username = delBtn.dataset.username || `#${userId}`;
            const dialog = document.getElementById("adminDeleteUserDialog");
            const promptEl = document.getElementById("adminDeleteUserPrompt");
            const confirmBtn = document.getElementById("confirmAdminDeleteUserButton");
            const cancelBtn = document.getElementById("cancelAdminDeleteUserButton");
            const closeBtn = document.getElementById("closeAdminDeleteUserDialog");

            if (!dialog) return;
            if (promptEl) {
                promptEl.textContent = `Are you sure you want to permanently delete account "${username}" (#${userId})?`;
            }

            const cleanup = () => {
                dialog.close();
                confirmBtn?.removeEventListener("click", onConfirm);
                cancelBtn?.removeEventListener("click", onCancel);
                closeBtn?.removeEventListener("click", onCancel);
            };
            const onCancel = () => cleanup();
            const onConfirm = async () => {
                cleanup();
                try {
                    await adminApiCall(`/api/admin/users/${encodeURIComponent(userId)}`, "DELETE");
                    setAdminMessage(adminUsersMessage, `Account #${userId} and history permanently deleted.`);
                    await loadAdminUsers();
                } catch (error) {
                    setAdminMessage(adminUsersMessage, error.message, true);
                }
            };

            confirmBtn?.addEventListener("click", onConfirm, { once: true });
            cancelBtn?.addEventListener("click", onCancel, { once: true });
            closeBtn?.addEventListener("click", onCancel, { once: true });
            dialog.showModal();
            return;
        }

        const button = event.target.closest(".admin-user-status-btn");
        if (!button) return;
        const userId = button.dataset.userId;
        const currentlyActive = button.dataset.active === "true";
        try {
            await adminApiCall(`/api/admin/users/${encodeURIComponent(userId)}/status`, "PUT", { is_active: !currentlyActive });
            setAdminMessage(adminUsersMessage, `Account #${userId} ${!currentlyActive ? "activated" : "deactivated"}.`);
            await loadAdminUsers();
        } catch (error) {
            setAdminMessage(adminUsersMessage, error.message, true);
        }
    });
}

// ---------------------------------------------------------------------
// 2. Parking Areas Management
// ---------------------------------------------------------------------
async function loadAdminAreas() {
    if (!adminAreasListEl) return;
    adminAreasListEl.innerHTML = `<tr><td colspan="8" class="muted">Loading areas...</td></tr>`;
    try {
        const data = await adminApiCall("/api/admin/parking-areas");
        adminAreasCache = data.areas || [];
        if (adminAreasCache.length === 0) {
            adminAreasListEl.innerHTML = `<tr><td colspan="8" class="muted">No parking areas found.</td></tr>`;
            return;
        }
        adminAreasListEl.innerHTML = adminAreasCache.map((area) => `
            <tr>
                <td><strong>#${area.area_id}</strong></td>
                <td><strong>${escapeHtml(area.name)}</strong></td>
                <td>${escapeHtml(area.location)}</td>
                <td>${escapeHtml(area.operating_hours || "—")}</td>
                <td>${area.capacity}</td>
                <td>${area.slot_count}</td>
                <td><span class="badge badge-${escapeHtml(area.status)}">${escapeHtml(area.status)}</span></td>
                <td>
                    <div class="admin-actions-cell">
                        <button type="button" class="admin-edit-area-btn" data-area-id="${area.area_id}">Edit</button>
                        <button type="button" class="danger admin-delete-area-btn" data-area-id="${area.area_id}">Delete</button>
                    </div>
                </td>
            </tr>
        `).join("");
    } catch (error) {
        setAdminMessage(adminAreasMessage, error.message, true);
        adminAreasListEl.innerHTML = `<tr><td colspan="8" class="muted is-error">Failed to load areas: ${escapeHtml(error.message)}</td></tr>`;
    }
}

if (adminAreaForm) {
    adminAreaForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        const formData = new FormData(adminAreaForm);
        const areaId = formData.get("area_id");
        const payload = {
            name: formData.get("name")?.trim(),
            location: formData.get("location")?.trim(),
            operating_hours: formData.get("operating_hours")?.trim(),
            capacity: Number(formData.get("capacity")),
            status: formData.get("status")
        };
        const lat = formData.get("latitude");
        const lng = formData.get("longitude");
        if (lat) payload.latitude = Number(lat);
        if (lng) payload.longitude = Number(lng);

        try {
            if (areaId) {
                await adminApiCall(`/api/admin/parking-areas/${encodeURIComponent(areaId)}`, "PUT", payload);
                setAdminMessage(adminAreasMessage, `Area #${areaId} updated successfully.`);
            } else {
                await adminApiCall("/api/admin/parking-areas", "POST", payload);
                setAdminMessage(adminAreasMessage, "New parking area created.");
            }
            resetAreaForm();
            await loadAdminAreas();
            if (typeof loadAreas === "function") await loadAreas();
        } catch (error) {
            setAdminMessage(adminAreasMessage, error.message, true);
        }
    });
}

function resetAreaForm() {
    if (!adminAreaForm) return;
    adminAreaForm.reset();
    adminAreaForm.elements.area_id.value = "";
    if (saveAreaButton) saveAreaButton.textContent = "Save Area";
    if (cancelAreaEdit) cancelAreaEdit.hidden = true;
}

if (cancelAreaEdit) {
    cancelAreaEdit.addEventListener("click", resetAreaForm);
}

if (adminAreasListEl) {
    adminAreasListEl.addEventListener("click", async (event) => {
        const editBtn = event.target.closest(".admin-edit-area-btn");
        if (editBtn) {
            const areaId = editBtn.dataset.areaId;
            const area = adminAreasCache.find((a) => String(a.area_id) === String(areaId));
            if (!area) return;
            adminAreaForm.elements.area_id.value = area.area_id;
            adminAreaForm.elements.name.value = area.name;
            adminAreaForm.elements.location.value = area.location;
            adminAreaForm.elements.operating_hours.value = area.operating_hours || "";
            adminAreaForm.elements.capacity.value = area.capacity;
            adminAreaForm.elements.status.value = area.status;
            if (saveAreaButton) saveAreaButton.textContent = "Update Area";
            if (cancelAreaEdit) cancelAreaEdit.hidden = false;
            adminAreaForm.scrollIntoView({ behavior: "smooth", block: "nearest" });
            return;
        }

        const deleteBtn = event.target.closest(".admin-delete-area-btn");
        if (deleteBtn) {
            const areaId = deleteBtn.dataset.areaId;
            const confirmed = await showAppConfirm({
                title: "Delete Parking Area?",
                message: `Are you sure you want to delete parking area #${areaId}? All associated slots will also be affected.`,
                type: "danger",
                confirmText: "Delete Area"
            });
            if (!confirmed) return;
            try {
                await adminApiCall(`/api/admin/parking-areas/${encodeURIComponent(areaId)}`, "DELETE");
                setAdminMessage(adminAreasMessage, `Parking area #${areaId} deleted.`);
                await loadAdminAreas();
                if (typeof loadAreas === "function") await loadAreas();
            } catch (error) {
                setAdminMessage(adminAreasMessage, error.message, true);
            }
        }
    });
}

// ---------------------------------------------------------------------
// 3. Parking Slots Management
// ---------------------------------------------------------------------
async function loadAdminSlotsTab() {
    try {
        const areasData = await adminApiCall("/api/admin/parking-areas");
        adminAreasCache = areasData.areas || [];
        if (adminSlotAreaSelect) {
            const currentSelected = adminSlotAreaSelect.value;
            adminSlotAreaSelect.innerHTML = adminAreasCache.map((a) => `
                <option value="${a.area_id}">${escapeHtml(a.name)} (${a.slot_count} slots)</option>
            `).join("");
            if (currentSelected && adminAreasCache.some((a) => String(a.area_id) === currentSelected)) {
                adminSlotAreaSelect.value = currentSelected;
            }
        }
        if (adminSlotAreaSelect && adminSlotAreaSelect.value) {
            await loadAdminSlots(adminSlotAreaSelect.value);
        } else if (adminSlotsListEl) {
            adminSlotsListEl.innerHTML = `<tr><td colspan="7" class="muted">No parking areas available.</td></tr>`;
        }
    } catch (error) {
        setAdminMessage(adminSlotsMessage, error.message, true);
    }
}

if (adminSlotAreaSelect) {
    adminSlotAreaSelect.addEventListener("change", () => {
        resetSlotForm();
        loadAdminSlots(adminSlotAreaSelect.value);
    });
}

async function loadAdminSlots(areaId) {
    if (!adminSlotsListEl || !areaId) return;
    adminSlotsListEl.innerHTML = `<tr><td colspan="7" class="muted">Loading slots...</td></tr>`;
    try {
        const data = await adminApiCall(`/api/admin/parking-areas/${encodeURIComponent(areaId)}/slots`);
        adminSlotsCache = data.slots || [];
        if (adminSlotsCache.length === 0) {
            adminSlotsListEl.innerHTML = `<tr><td colspan="7" class="muted">No slots in this parking area.</td></tr>`;
            return;
        }
        adminSlotsListEl.innerHTML = adminSlotsCache.map((slot) => `
            <tr>
                <td><strong>#${slot.slot_id}</strong></td>
                <td><strong>${escapeHtml(slot.slot_number)}</strong></td>
                <td>${escapeHtml(slot.vehicle_type)}</td>
                <td>${escapeHtml(slot.slot_type)}</td>
                <td>${slot.is_ev ? '<span class="feature-tag">⚡ EV</span>' : '<span class="muted">No</span>'}</td>
                <td><span class="status-pill ${escapeHtml(slot.status)}">${escapeHtml(slot.status)}</span></td>
                <td>
                    <div class="admin-actions-cell">
                        <button type="button" class="admin-edit-slot-btn" data-slot-id="${slot.slot_id}">Edit</button>
                        <button type="button" class="danger admin-delete-slot-btn" data-slot-id="${slot.slot_id}">Delete</button>
                    </div>
                </td>
            </tr>
        `).join("");
    } catch (error) {
        setAdminMessage(adminSlotsMessage, error.message, true);
        adminSlotsListEl.innerHTML = `<tr><td colspan="7" class="muted is-error">Failed to load slots: ${escapeHtml(error.message)}</td></tr>`;
    }
}

if (adminSlotForm) {
    adminSlotForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        const areaId = adminSlotAreaSelect.value;
        if (!areaId) {
            setAdminMessage(adminSlotsMessage, "Select a parking area first.", true);
            return;
        }
        const formData = new FormData(adminSlotForm);
        const slotId = formData.get("slot_id");
        const payload = {
            slot_number: formData.get("slot_number")?.trim(),
            vehicle_type: formData.get("vehicle_type"),
            slot_type: formData.get("slot_type")?.trim(),
            is_ev: formData.get("is_ev") === "on",
            status: formData.get("status")
        };

        try {
            if (slotId) {
                await adminApiCall(`/api/admin/parking-slots/${encodeURIComponent(slotId)}`, "PUT", payload);
                setAdminMessage(adminSlotsMessage, `Slot #${slotId} updated.`);
            } else {
                await adminApiCall(`/api/admin/parking-areas/${encodeURIComponent(areaId)}/slots`, "POST", payload);
                setAdminMessage(adminSlotsMessage, `Slot ${payload.slot_number} added.`);
            }
            resetSlotForm();
            await loadAdminSlots(areaId);
            if (typeof loadAreas === "function") await loadAreas();
        } catch (error) {
            setAdminMessage(adminSlotsMessage, error.message, true);
        }
    });
}

function resetSlotForm() {
    if (!adminSlotForm) return;
    adminSlotForm.reset();
    adminSlotForm.elements.slot_id.value = "";
    if (saveSlotButton) saveSlotButton.textContent = "Add Slot";
    if (cancelSlotEdit) cancelSlotEdit.hidden = true;
}

if (cancelSlotEdit) {
    cancelSlotEdit.addEventListener("click", resetSlotForm);
}

if (adminSlotsListEl) {
    adminSlotsListEl.addEventListener("click", async (event) => {
        const editBtn = event.target.closest(".admin-edit-slot-btn");
        if (editBtn) {
            const slotId = editBtn.dataset.slotId;
            const slot = adminSlotsCache.find((s) => String(s.slot_id) === String(slotId));
            if (!slot) return;
            adminSlotForm.elements.slot_id.value = slot.slot_id;
            adminSlotForm.elements.slot_number.value = slot.slot_number;
            adminSlotForm.elements.vehicle_type.value = slot.vehicle_type;
            adminSlotForm.elements.slot_type.value = slot.slot_type;
            adminSlotForm.elements.is_ev.checked = slot.is_ev;
            adminSlotForm.elements.status.value = slot.status;
            if (saveSlotButton) saveSlotButton.textContent = "Update Slot";
            if (cancelSlotEdit) cancelSlotEdit.hidden = false;
            adminSlotForm.scrollIntoView({ behavior: "smooth", block: "nearest" });
            return;
        }

        const deleteBtn = event.target.closest(".admin-delete-slot-btn");
        if (deleteBtn) {
            const slotId = deleteBtn.dataset.slotId;
            const confirmed = await showAppConfirm({
                title: "Delete Slot?",
                message: `Are you sure you want to delete slot #${slotId}?`,
                type: "danger",
                confirmText: "Delete Slot"
            });
            if (!confirmed) return;
            try {
                await adminApiCall(`/api/admin/parking-slots/${encodeURIComponent(slotId)}`, "DELETE");
                setAdminMessage(adminSlotsMessage, `Slot #${slotId} deleted.`);
                await loadAdminSlots(adminSlotAreaSelect.value);
                if (typeof loadAreas === "function") await loadAreas();
            } catch (error) {
                setAdminMessage(adminSlotsMessage, error.message, true);
            }
        }
    });
}

// ---------------------------------------------------------------------
// 4. Parking Rates Management
// ---------------------------------------------------------------------
async function loadAdminRatesTab() {
    try {
        const areasData = await adminApiCall("/api/admin/parking-areas");
        adminAreasCache = areasData.areas || [];
        if (adminRateAreaSelect) {
            adminRateAreaSelect.innerHTML = adminAreasCache.map((a) => `
                <option value="${a.area_id}">${escapeHtml(a.name)}</option>
            `).join("");
        }
        await loadAdminRates();
    } catch (error) {
        setAdminMessage(adminRatesMessage, error.message, true);
    }
}

async function loadAdminRates() {
    if (!adminRatesListEl) return;
    adminRatesListEl.innerHTML = `<tr><td colspan="8" class="muted">Loading rates...</td></tr>`;
    try {
        const data = await adminApiCall("/api/admin/parking-rates");
        adminRatesCache = data.rates || [];
        if (adminRatesCache.length === 0) {
            adminRatesListEl.innerHTML = `<tr><td colspan="8" class="muted">No parking rates defined yet.</td></tr>`;
            return;
        }
        adminRatesListEl.innerHTML = adminRatesCache.map((rate) => `
            <tr>
                <td><strong>#${rate.rate_id}</strong></td>
                <td>${escapeHtml(rate.area_name)}</td>
                <td>${escapeHtml(rate.vehicle_type)}</td>
                <td>${escapeHtml(rate.slot_type)}</td>
                <td><strong>₹${Number(rate.hourly_rate).toFixed(2)}</strong></td>
                <td>₹${Number(rate.overstay_rate).toFixed(2)}</td>
                <td>₹${Number(rate.ev_charging_rate).toFixed(2)}</td>
                <td>
                    <div class="admin-actions-cell">
                        <button type="button" class="admin-edit-rate-btn" data-rate-id="${rate.rate_id}">Edit</button>
                        <button type="button" class="danger admin-delete-rate-btn" data-rate-id="${rate.rate_id}">Delete</button>
                    </div>
                </td>
            </tr>
        `).join("");
    } catch (error) {
        setAdminMessage(adminRatesMessage, error.message, true);
        adminRatesListEl.innerHTML = `<tr><td colspan="8" class="muted is-error">Failed to load rates: ${escapeHtml(error.message)}</td></tr>`;
    }
}

if (adminRateForm) {
    adminRateForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        const formData = new FormData(adminRateForm);
        const rateId = formData.get("rate_id");
        const payload = {
            area_id: Number(formData.get("area_id")),
            vehicle_type: formData.get("vehicle_type"),
            slot_type: formData.get("slot_type")?.trim(),
            hourly_rate: Number(formData.get("hourly_rate")),
            overstay_rate: Number(formData.get("overstay_rate")),
            ev_charging_rate: Number(formData.get("ev_charging_rate"))
        };

        try {
            if (rateId) {
                await adminApiCall(`/api/admin/parking-rates/${encodeURIComponent(rateId)}`, "PUT", payload);
                setAdminMessage(adminRatesMessage, `Rate #${rateId} updated.`);
            } else {
                await adminApiCall("/api/admin/parking-rates", "POST", payload);
                setAdminMessage(adminRatesMessage, "New parking rate rule created.");
            }
            resetRateForm();
            await loadAdminRates();
        } catch (error) {
            setAdminMessage(adminRatesMessage, error.message, true);
        }
    });
}

function resetRateForm() {
    if (!adminRateForm) return;
    adminRateForm.reset();
    adminRateForm.elements.rate_id.value = "";
    if (saveRateButton) saveRateButton.textContent = "Save Rate";
    if (cancelRateEdit) cancelRateEdit.hidden = true;
}

if (cancelRateEdit) {
    cancelRateEdit.addEventListener("click", resetRateForm);
}

if (adminRatesListEl) {
    adminRatesListEl.addEventListener("click", async (event) => {
        const editBtn = event.target.closest(".admin-edit-rate-btn");
        if (editBtn) {
            const rateId = editBtn.dataset.rateId;
            const rate = adminRatesCache.find((r) => String(r.rate_id) === String(rateId));
            if (!rate) return;
            adminRateForm.elements.rate_id.value = rate.rate_id;
            adminRateForm.elements.area_id.value = rate.area_id;
            adminRateForm.elements.vehicle_type.value = rate.vehicle_type;
            adminRateForm.elements.slot_type.value = rate.slot_type;
            adminRateForm.elements.hourly_rate.value = rate.hourly_rate;
            adminRateForm.elements.overstay_rate.value = rate.overstay_rate;
            adminRateForm.elements.ev_charging_rate.value = rate.ev_charging_rate;
            if (saveRateButton) saveRateButton.textContent = "Update Rate";
            if (cancelRateEdit) cancelRateEdit.hidden = false;
            adminRateForm.scrollIntoView({ behavior: "smooth", block: "nearest" });
            return;
        }

        const deleteBtn = event.target.closest(".admin-delete-rate-btn");
        if (deleteBtn) {
            const rateId = deleteBtn.dataset.rateId;
            const confirmed = await showAppConfirm({
                title: "Delete Rate Rule?",
                message: `Are you sure you want to delete rate rule #${rateId}?`,
                type: "danger",
                confirmText: "Delete Rate"
            });
            if (!confirmed) return;
            try {
                await adminApiCall(`/api/admin/parking-rates/${encodeURIComponent(rateId)}`, "DELETE");
                setAdminMessage(adminRatesMessage, `Parking rate #${rateId} deleted.`);
                await loadAdminRates();
            } catch (error) {
                setAdminMessage(adminRatesMessage, error.message, true);
            }
        }
    });
}

// ---------------------------------------------------------------------
// 5. Slot Maintenance Management
// ---------------------------------------------------------------------
async function loadAdminMaintenanceTab() {
    try {
        const areasData = await adminApiCall("/api/admin/parking-areas");
        const areas = areasData.areas || [];
        const slotPromises = areas.map((a) => adminApiCall(`/api/admin/parking-areas/${a.area_id}/slots`).catch(() => ({ slots: [] })));
        const slotsResults = await Promise.all(slotPromises);
        const allSlots = slotsResults.flatMap((r) => r.slots || []);

        if (adminMaintenanceSlotSelect) {
            adminMaintenanceSlotSelect.innerHTML = allSlots.map((s) => `
                <option value="${s.slot_id}">${escapeHtml(s.area_name)} · Slot ${escapeHtml(s.slot_number)} (${escapeHtml(s.status)})</option>
            `).join("");
        }

        await loadAdminMaintenance();
    } catch (error) {
        setAdminMessage(adminMaintenanceMessage, error.message, true);
    }
}

async function loadAdminMaintenance() {
    if (!adminMaintenanceListEl) return;
    adminMaintenanceListEl.innerHTML = `<tr><td colspan="7" class="muted">Loading maintenance records...</td></tr>`;
    try {
        const data = await adminApiCall("/api/admin/maintenance");
        adminMaintenanceCache = data.maintenance || [];
        if (adminMaintenanceCache.length === 0) {
            adminMaintenanceListEl.innerHTML = `<tr><td colspan="7" class="muted">No maintenance records scheduled.</td></tr>`;
            return;
        }
        adminMaintenanceListEl.innerHTML = adminMaintenanceCache.map((m) => `
            <tr>
                <td><strong>#${m.maintenance_id}</strong></td>
                <td><strong>${escapeHtml(m.area_name || "Area")} · ${escapeHtml(m.slot_number || `Slot #${m.slot_id}`)}</strong></td>
                <td>${escapeHtml(m.reason)}</td>
                <td>${escapeHtml(formatIsoTimestamp(m.start_time))}</td>
                <td>${m.end_time ? escapeHtml(formatIsoTimestamp(m.end_time)) : '<span class="muted">Open-ended</span>'}</td>
                <td><span class="status-pill ${m.status === 'in_progress' ? 'occupied' : (m.status === 'scheduled' ? 'reserved' : (m.status === 'completed' ? 'available' : 'unavailable'))}">${escapeHtml(m.status)}</span></td>
                <td>
                    <div class="admin-actions-cell">
                        <button type="button" class="admin-edit-maint-btn" data-maint-id="${m.maintenance_id}">Edit</button>
                        <button type="button" class="danger admin-delete-maint-btn" data-maint-id="${m.maintenance_id}">Delete</button>
                    </div>
                </td>
            </tr>
        `).join("");
    } catch (error) {
        setAdminMessage(adminMaintenanceMessage, error.message, true);
        adminMaintenanceListEl.innerHTML = `<tr><td colspan="7" class="muted is-error">Failed to load maintenance: ${escapeHtml(error.message)}</td></tr>`;
    }
}

if (adminMaintenanceForm) {
    adminMaintenanceForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        const formData = new FormData(adminMaintenanceForm);
        const maintId = formData.get("maintenance_id");
        const startTimeInput = formData.get("start_time");
        const endTimeInput = formData.get("end_time");

        const payload = {
            slot_id: Number(formData.get("slot_id")),
            reason: formData.get("reason")?.trim(),
            start_time: startTimeInput ? new Date(startTimeInput).toISOString() : "",
            end_time: endTimeInput ? new Date(endTimeInput).toISOString() : "",
            status: formData.get("status")
        };

        try {
            if (maintId) {
                await adminApiCall(`/api/admin/maintenance/${encodeURIComponent(maintId)}`, "PUT", payload);
                setAdminMessage(adminMaintenanceMessage, `Maintenance #${maintId} updated.`);
            } else {
                await adminApiCall("/api/admin/maintenance", "POST", payload);
                setAdminMessage(adminMaintenanceMessage, "Maintenance schedule recorded.");
            }
            resetMaintenanceForm();
            await loadAdminMaintenance();
            if (typeof loadAreas === "function") await loadAreas();
        } catch (error) {
            setAdminMessage(adminMaintenanceMessage, error.message, true);
        }
    });
}

function resetMaintenanceForm() {
    if (!adminMaintenanceForm) return;
    adminMaintenanceForm.reset();
    adminMaintenanceForm.elements.maintenance_id.value = "";
    if (saveMaintenanceButton) saveMaintenanceButton.textContent = "Save Schedule";
    if (cancelMaintenanceEdit) cancelMaintenanceEdit.hidden = true;
}

if (cancelMaintenanceEdit) {
    cancelMaintenanceEdit.addEventListener("click", resetMaintenanceForm);
}

if (adminMaintenanceListEl) {
    adminMaintenanceListEl.addEventListener("click", async (event) => {
        const editBtn = event.target.closest(".admin-edit-maint-btn");
        if (editBtn) {
            const maintId = editBtn.dataset.maintId;
            const m = adminMaintenanceCache.find((item) => String(item.maintenance_id) === String(maintId));
            if (!m) return;
            adminMaintenanceForm.elements.maintenance_id.value = m.maintenance_id;
            if (adminMaintenanceSlotSelect) adminMaintenanceSlotSelect.value = m.slot_id;
            adminMaintenanceForm.elements.reason.value = m.reason;
            adminMaintenanceForm.elements.start_time.value = toLocalDatetimeInputValue(m.start_time);
            adminMaintenanceForm.elements.end_time.value = toLocalDatetimeInputValue(m.end_time);
            adminMaintenanceForm.elements.status.value = m.status;
            if (saveMaintenanceButton) saveMaintenanceButton.textContent = "Update Schedule";
            if (cancelMaintenanceEdit) cancelMaintenanceEdit.hidden = false;
            adminMaintenanceForm.scrollIntoView({ behavior: "smooth", block: "nearest" });
            return;
        }

        const deleteBtn = event.target.closest(".admin-delete-maint-btn");
        if (deleteBtn) {
            const maintId = deleteBtn.dataset.maintId;
            const confirmed = await showAppConfirm({
                title: "Delete Maintenance Record?",
                message: `Are you sure you want to delete maintenance record #${maintId}?`,
                type: "danger",
                confirmText: "Delete Record"
            });
            if (!confirmed) return;
            try {
                await adminApiCall(`/api/admin/maintenance/${encodeURIComponent(maintId)}`, "DELETE");
                setAdminMessage(adminMaintenanceMessage, `Maintenance #${maintId} removed.`);
                await loadAdminMaintenance();
                if (typeof loadAreas === "function") await loadAreas();
            } catch (error) {
                setAdminMessage(adminMaintenanceMessage, error.message, true);
            }
        }
    });
}

