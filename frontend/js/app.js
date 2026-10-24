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

function showDashboardPage(page) {
    const showMap = page === "map";
    if (activityPage) activityPage.hidden = showMap;
    if (mapPage) mapPage.hidden = !showMap;
    if (activityPageButton) {
        activityPageButton.classList.toggle("active", !showMap);
        if (!showMap) activityPageButton.setAttribute("aria-current", "page");
        else activityPageButton.removeAttribute("aria-current");
    }
    if (mapPageButton) {
        mapPageButton.classList.toggle("active", showMap);
        if (showMap) mapPageButton.setAttribute("aria-current", "page");
        else mapPageButton.removeAttribute("aria-current");
    }
}

activityPageButton?.addEventListener("click", () => showDashboardPage("activity"));
mapPageButton?.addEventListener("click", () => showDashboardPage("map"));

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
