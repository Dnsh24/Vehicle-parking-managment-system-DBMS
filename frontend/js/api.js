// =====================================================================
// ParkFlow - Shared API & Helper Utilities
// =====================================================================

const API_BASE = "http://localhost:3000";

function authHeaders() {
    const token = window.localStorage.getItem("parking_auth_token") || window.sessionStorage.getItem("parking_auth_token");
    return token ? { Authorization: `Bearer ${token}` } : {};
}

function escapeHtml(value) {
    if (value === null || value === undefined) return "";
    return String(value)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");
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
    if (slot.is_ev || /\bev\b|electric|charg/i.test(slot.slot_type)) {
        return "ev-car";
    }
    if (slotType.includes("handicap") || slotType.includes("accessible")) {
        return "accessible-car";
    }
    return "car";
}

function formatIsoTimestamp(iso) {
    if (!iso) return "";
    try {
        const d = new Date(iso);
        return d.toLocaleString();
    } catch {
        return iso;
    }
}

function toLocalDatetimeInputValue(iso) {
    if (!iso) return "";
    try {
        const d = new Date(iso);
        const pad = (n) => String(n).padStart(2, '0');
        return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
    } catch {
        return "";
    }
}

async function adminApiCall(endpoint, method = "GET", body = null) {
    const options = {
        method,
        headers: {
            ...authHeaders(),
            "Content-Type": "application/json"
        }
    };
    if (body) {
        options.body = JSON.stringify(body);
    }
    const response = await fetch(`${API_BASE}${endpoint}`, options);
    if (response.status === 204) {
        return null;
    }
    let data;
    try {
        data = await response.json();
    } catch {
        data = {};
    }
    if (!response.ok) {
        throw new Error(data.error?.message || `Admin request failed (${response.status})`);
    }
    return data;
}


// =====================================================================
// In-App Toast & Modal Notice System
// =====================================================================

function showToast(message, type = "info", title = "") {
    const container = document.getElementById("toastContainer");
    if (!container || !message) return;

    const toast = document.createElement("div");
    toast.className = `toast toast-${type}`;

    let icon = "ℹ️";
    let defaultTitle = "Notice";
    if (type === "success") {
        icon = "✅";
        defaultTitle = "Success";
    } else if (type === "warning") {
        icon = "⏰";
        defaultTitle = "Attention";
    } else if (type === "error" || type === "danger") {
        icon = "❌";
        defaultTitle = "Error";
    }

    const iconEl = document.createElement("div");
    iconEl.className = "toast-icon";
    iconEl.textContent = icon;

    const contentEl = document.createElement("div");
    contentEl.className = "toast-content";

    const titleEl = document.createElement("div");
    titleEl.className = "toast-title";
    titleEl.textContent = title || defaultTitle;

    const msgEl = document.createElement("div");
    msgEl.className = "toast-message";
    msgEl.textContent = message;

    contentEl.append(titleEl, msgEl);

    const closeBtn = document.createElement("button");
    closeBtn.className = "toast-close";
    closeBtn.type = "button";
    closeBtn.textContent = "✕";
    closeBtn.setAttribute("aria-label", "Close notification");

    const removeToast = () => {
        if (toast.classList.contains("toast-closing")) return;
        toast.classList.add("toast-closing");
        setTimeout(() => toast.remove(), 220);
    };

    closeBtn.addEventListener("click", removeToast);
    setTimeout(removeToast, 5000);

    toast.append(iconEl, contentEl, closeBtn);
    container.prepend(toast);
}

function showAppNotice({
    title = "Notice",
    message = "",
    type = "info", // "info" | "success" | "warning" | "danger"
    subtitle = "",
    details = null, // array of { label, value } or object
    confirmText = "Got It"
} = {}) {
    return new Promise((resolve) => {
        const dialog = document.getElementById("appNoticeDialog");
        if (!dialog) {
            window.alert(`${title}\n\n${message}`);
            resolve(true);
            return;
        }

        const iconEl = document.getElementById("appNoticeIcon");
        const titleEl = document.getElementById("appNoticeTitle");
        const subtitleEl = document.getElementById("appNoticeSubtitle");
        const messageEl = document.getElementById("appNoticeMessage");
        const detailsEl = document.getElementById("appNoticeDetails");
        const confirmBtn = document.getElementById("confirmAppNoticeButton");
        const cancelBtn = document.getElementById("cancelAppNoticeButton");
        const closeBtn = document.getElementById("closeAppNoticeButton");

        if (titleEl) titleEl.textContent = title;
        if (subtitleEl) {
            subtitleEl.textContent = subtitle;
            subtitleEl.style.display = subtitle ? "block" : "none";
        }
        if (messageEl) messageEl.textContent = message;

        // Icons & style mapping
        let emoji = "ℹ️";
        let badgeClass = "is-info";
        if (type === "success") {
            emoji = "✅";
            badgeClass = "is-success";
        } else if (type === "warning") {
            emoji = "⏰";
            badgeClass = "is-warning";
        } else if (type === "danger" || type === "error") {
            emoji = "❌";
            badgeClass = "is-danger";
        }
        if (iconEl) {
            iconEl.className = `app-notice-icon-badge ${badgeClass}`;
            iconEl.textContent = emoji;
        }

        // Details breakdown
        if (detailsEl) {
            detailsEl.replaceChildren();
            if (details && Array.isArray(details) && details.length > 0) {
                detailsEl.style.display = "flex";
                details.forEach((row) => {
                    const rowEl = document.createElement("div");
                    rowEl.className = "app-notice-row";
                    const labelEl = document.createElement("span");
                    labelEl.className = "label";
                    labelEl.textContent = row.label;
                    const valEl = document.createElement("span");
                    valEl.className = "val";
                    valEl.textContent = row.value;
                    rowEl.append(labelEl, valEl);
                    detailsEl.append(rowEl);
                });
            } else if (details && typeof details === "object" && Object.keys(details).length > 0) {
                detailsEl.style.display = "flex";
                Object.entries(details).forEach(([key, val]) => {
                    const rowEl = document.createElement("div");
                    rowEl.className = "app-notice-row";
                    const labelEl = document.createElement("span");
                    labelEl.className = "label";
                    labelEl.textContent = key;
                    const valEl = document.createElement("span");
                    valEl.className = "val";
                    valEl.textContent = String(val);
                    rowEl.append(labelEl, valEl);
                    detailsEl.append(rowEl);
                });
            } else {
                detailsEl.style.display = "none";
            }
        }

        if (confirmBtn) {
            confirmBtn.textContent = confirmText;
            confirmBtn.className = type === "danger" || type === "error" ? "danger" : "refresh-button";
        }
        if (cancelBtn) {
            cancelBtn.style.display = "none";
        }

        const cleanup = () => {
            confirmBtn?.removeEventListener("click", onConfirm);
            cancelBtn?.removeEventListener("click", onCancel);
            closeBtn?.removeEventListener("click", onCancel);
            dialog.removeEventListener("cancel", onCancel);
        };

        const onConfirm = () => {
            cleanup();
            dialog.close();
            resolve(true);
        };

        const onCancel = () => {
            cleanup();
            dialog.close();
            resolve(false);
        };

        confirmBtn?.addEventListener("click", onConfirm);
        closeBtn?.addEventListener("click", onCancel);
        dialog.addEventListener("cancel", onCancel);

        dialog.showModal();
        confirmBtn?.focus();
    });
}

function showAppConfirm({
    title = "Confirm Action",
    message = "",
    type = "warning",
    confirmText = "Confirm",
    cancelText = "Cancel",
    details = null
} = {}) {
    return new Promise((resolve) => {
        const dialog = document.getElementById("appNoticeDialog");
        if (!dialog) {
            resolve(window.confirm(`${title}\n\n${message}`));
            return;
        }

        const iconEl = document.getElementById("appNoticeIcon");
        const titleEl = document.getElementById("appNoticeTitle");
        const subtitleEl = document.getElementById("appNoticeSubtitle");
        const messageEl = document.getElementById("appNoticeMessage");
        const detailsEl = document.getElementById("appNoticeDetails");
        const confirmBtn = document.getElementById("confirmAppNoticeButton");
        const cancelBtn = document.getElementById("cancelAppNoticeButton");
        const closeBtn = document.getElementById("closeAppNoticeButton");

        if (titleEl) titleEl.textContent = title;
        if (subtitleEl) subtitleEl.style.display = "none";
        if (messageEl) messageEl.textContent = message;

        let emoji = type === "danger" ? "⚠️" : "❓";
        let badgeClass = type === "danger" ? "is-danger" : "is-warning";
        if (iconEl) {
            iconEl.className = `app-notice-icon-badge ${badgeClass}`;
            iconEl.textContent = emoji;
        }

        if (detailsEl) {
            detailsEl.replaceChildren();
            if (details && Array.isArray(details) && details.length > 0) {
                detailsEl.style.display = "flex";
                details.forEach((row) => {
                    const rowEl = document.createElement("div");
                    rowEl.className = "app-notice-row";
                    const labelEl = document.createElement("span");
                    labelEl.className = "label";
                    labelEl.textContent = row.label;
                    const valEl = document.createElement("span");
                    valEl.className = "val";
                    valEl.textContent = row.value;
                    rowEl.append(labelEl, valEl);
                    detailsEl.append(rowEl);
                });
            } else {
                detailsEl.style.display = "none";
            }
        }

        if (cancelBtn) {
            cancelBtn.textContent = cancelText;
            cancelBtn.style.display = "inline-block";
        }
        if (confirmBtn) {
            confirmBtn.textContent = confirmText;
            confirmBtn.className = type === "danger" ? "danger" : "refresh-button";
        }

        const cleanup = () => {
            confirmBtn?.removeEventListener("click", onConfirm);
            cancelBtn?.removeEventListener("click", onCancel);
            closeBtn?.removeEventListener("click", onCancel);
            dialog.removeEventListener("cancel", onCancel);
        };

        const onConfirm = () => {
            cleanup();
            dialog.close();
            resolve(true);
        };

        const onCancel = () => {
            cleanup();
            dialog.close();
            resolve(false);
        };

        confirmBtn?.addEventListener("click", onConfirm);
        cancelBtn?.addEventListener("click", onCancel);
        closeBtn?.addEventListener("click", onCancel);
        dialog.addEventListener("cancel", onCancel);

        dialog.showModal();
        confirmBtn?.focus();
    });
}
