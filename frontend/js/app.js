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

if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initParkFlowApp);
} else {
    initParkFlowApp();
}
