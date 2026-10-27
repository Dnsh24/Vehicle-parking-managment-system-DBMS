// =====================================================================
// ParkFlow - Main Application Bootstrap
// =====================================================================

/**
 * Initializes ParkFlow application by checking for an existing
 * authentication session and loading initial dashboard state.
 */
function initParkFlowApp() {
    restoreSession();
}

const activityPage = document.getElementById("activityPage");
const mapPage = document.getElementById("mapPage");
const activityPageButton = document.getElementById("activityPageButton");
const mapPageButton = document.getElementById("mapPageButton");
const notificationsPageButton = document.getElementById("notificationsPageButton");
const notificationsPage = document.getElementById("notificationsPage");

function showDashboardPage(page) {
    const showMap = page === "map";
    const showNotifications = page === "notifications";
    const showActivity = !showMap && !showNotifications;
    if (activityPage) activityPage.hidden = showMap || showNotifications;
    if (mapPage) mapPage.hidden = !showMap;
    if (notificationsPage) notificationsPage.hidden = !showNotifications;
    if (activityPageButton) {
        activityPageButton.classList.toggle("active", showActivity);
        if (showActivity) activityPageButton.setAttribute("aria-current", "page");
        else activityPageButton.removeAttribute("aria-current");
    }
    if (mapPageButton) {
        mapPageButton.classList.toggle("active", showMap);
        if (showMap) mapPageButton.setAttribute("aria-current", "page");
        else mapPageButton.removeAttribute("aria-current");
    }
    if (notificationsPageButton) {
        notificationsPageButton.classList.toggle("active", showNotifications);
        if (showNotifications) notificationsPageButton.setAttribute("aria-current", "page");
        else notificationsPageButton.removeAttribute("aria-current");
    }
}

activityPageButton?.addEventListener("click", () => showDashboardPage("activity"));
mapPageButton?.addEventListener("click", () => showDashboardPage("map"));
notificationsPageButton?.addEventListener("click", () => {
    showDashboardPage("notifications");
    if (typeof loadNotifications === "function") loadNotifications();
});

document.getElementById("refreshMapButton")?.addEventListener("click", async (event) => {
    const button = event.currentTarget;
    button.disabled = true;
    try {
        if (typeof loadAreas === "function") await loadAreas();
    } finally {
        button.disabled = false;
    }
});

if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initParkFlowApp);
} else {
    initParkFlowApp();
}
