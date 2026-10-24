// =====================================================================
// ParkFlow - Authentication Module
// =====================================================================

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
const accountButton = document.getElementById("accountButton");
const accountPopover = document.getElementById("accountPopover");
const topbar = document.getElementById("topbar");
const forgotPasswordButton = document.getElementById("forgotPasswordButton");
const passwordToggles = document.querySelectorAll("[data-password-toggle]");

const userLoginMode = document.getElementById("userLoginMode");
const adminLoginMode = document.getElementById("adminLoginMode");
const adminIndicator = document.getElementById("adminIndicator");
const loginHeading = document.getElementById("loginHeading");
const loginDescription = document.getElementById("loginDescription");
const loginEmailInput = document.getElementById("loginEmailInput");
const loginSubmitButton = document.getElementById("loginSubmitButton");

let currentUser = null;
let isAdminLoginMode = false;

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

function showAuthError(message) {
    if (authError) {
        authError.textContent = message;
    }
}

function setLoginMode(mode) {
    isAdminLoginMode = mode === "admin";
    showAuthError("");
    if (isAdminLoginMode) {
        if (userLoginMode) {
            userLoginMode.classList.remove("active");
            userLoginMode.setAttribute("aria-selected", "false");
        }
        if (adminLoginMode) {
            adminLoginMode.classList.add("active");
            adminLoginMode.setAttribute("aria-selected", "true");
        }
        if (loginScreen) loginScreen.classList.add("admin-mode-active");
        if (adminIndicator) adminIndicator.hidden = false;
        if (loginHeading) loginHeading.textContent = "Admin Log In";
        if (loginDescription) loginDescription.innerHTML = "Sign in with authorized administrator credentials<br />to access the ParkFlow management console.";
        if (loginEmailInput) loginEmailInput.placeholder = "Admin Email (dnshneela@gmail.com)";
        if (loginSubmitButton) loginSubmitButton.textContent = "Log In as Administrator";
        if (registerSwitch) registerSwitch.hidden = true;
    } else {
        if (adminLoginMode) {
            adminLoginMode.classList.remove("active");
            adminLoginMode.setAttribute("aria-selected", "false");
        }
        if (userLoginMode) {
            userLoginMode.classList.add("active");
            userLoginMode.setAttribute("aria-selected", "true");
        }
        if (loginScreen) loginScreen.classList.remove("admin-mode-active");
        if (adminIndicator) adminIndicator.hidden = true;
        if (loginHeading) loginHeading.textContent = "Log In";
        if (loginDescription) loginDescription.innerHTML = "Please enter your credentials to access<br />your dashboard.";
        if (loginEmailInput) loginEmailInput.placeholder = "Username or Email";
        if (loginSubmitButton) loginSubmitButton.textContent = "Log In";
        if (registerSwitch) registerSwitch.hidden = false;
    }
}

if (userLoginMode && adminLoginMode) {
    userLoginMode.addEventListener("click", () => setLoginMode("user"));
    adminLoginMode.addEventListener("click", () => setLoginMode("admin"));
}

function showAuthenticatedUser(user, openAdminDirectly = false) {
    currentUser = user;
    if (authGate) authGate.setAttribute("hidden", "");
    if (topbar) topbar.hidden = false;
    if (accountButton) {
        accountButton.textContent = user.name ? user.name.slice(0, 2).toUpperCase() : "U";
    }
    const accountName = document.getElementById("accountName");
    const accountEmail = document.getElementById("accountEmail");
    if (accountName) accountName.textContent = user.name || user.username || "Account";
    if (accountEmail) accountEmail.textContent = user.email || "";

    const adminBadge = document.getElementById("adminBadge");
    const viewSwitcher = document.getElementById("viewSwitcher");
    const viewUserMode = document.getElementById("viewUserMode");
    const viewAdminMode = document.getElementById("viewAdminMode");
    const adminConsole = document.getElementById("adminConsole");

    if (user.role === "admin") {
        if (adminBadge) adminBadge.hidden = false;
        if (viewSwitcher) viewSwitcher.hidden = false;
        if (openAdminDirectly) {
            if (typeof switchViewMode === "function") {
                switchViewMode("admin");
            }
        } else {
            if (protectedContent) protectedContent.removeAttribute("hidden");
            if (viewUserMode && viewAdminMode) {
                viewUserMode.classList.add("active");
                viewAdminMode.classList.remove("active");
            }
        }
    } else {
        if (adminBadge) adminBadge.hidden = true;
        if (viewSwitcher) viewSwitcher.hidden = true;
        if (adminConsole) adminConsole.hidden = true;
        if (protectedContent) protectedContent.removeAttribute("hidden");
        if (viewUserMode && viewAdminMode) {
            viewUserMode.classList.add("active");
            viewAdminMode.classList.remove("active");
        }
    }

    if (typeof loadAreas === "function") loadAreas();
    if (typeof loadVehicles === "function") loadVehicles();
    if (typeof loadReservationPage === "function") loadReservationPage();
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
        } catch {
            throw new Error("Cannot connect to the backend. Start parking_backend on port 3000.");
        }
        const responseText = await response.text();
        let data;
        try {
            data = responseText ? JSON.parse(responseText) : {};
        } catch {
            throw new Error(`Backend returned an invalid response (${response.status}).`);
        }
        if (!response.ok) {
            throw new Error(data.error?.message || "Authentication failed.");
        }
        const rememberMe = form.elements.remember_me?.checked ?? false;
        const tokenStorage = rememberMe ? window.localStorage : window.sessionStorage;
        const otherStorage = rememberMe ? window.sessionStorage : window.localStorage;

        if (isAdminLoginMode && data.user && data.user.role !== "admin") {
            throw new Error("This account does not have administrator privileges. Please use User Login.");
        }

        otherStorage.removeItem("parking_auth_token");
        tokenStorage.setItem("parking_auth_token", data.token);
        showAuthenticatedUser(data.user, isAdminLoginMode);
    } catch (error) {
        showAuthError(error.message);
    }
}

if (loginForm) {
    loginForm.addEventListener("submit", (event) => {
        event.preventDefault();
        authenticate("/api/auth/login", loginForm);
    });
}

if (registerForm) {
    registerForm.addEventListener("submit", (event) => {
        event.preventDefault();
        authenticate("/api/auth/register", registerForm);
    });
}

if (registerSwitch) registerSwitch.addEventListener("click", showRegisterScreen);
if (loginSwitch) loginSwitch.addEventListener("click", showLoginScreen);
if (forgotPasswordButton) {
    forgotPasswordButton.addEventListener("click", () => {
        showAuthError("Password reset is not available yet. Contact an administrator.");
    });
}

if (accountButton && accountPopover) {
    accountButton.addEventListener("click", (event) => {
        event.stopPropagation();
        const opening = accountPopover.hidden;
        accountPopover.hidden = !opening;
        accountButton.setAttribute("aria-expanded", String(opening));
        const notificationPopover = document.getElementById("notificationPopover");
        const notificationsButton = document.getElementById("notificationsButton");
        if (notificationPopover && !notificationPopover.hidden) {
            notificationPopover.hidden = true;
            notificationsButton?.setAttribute("aria-expanded", "false");
        }
    });
}

if (logoutButton) {
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
}

document.addEventListener("click", (event) => {
    if (accountPopover?.contains(event.target) || accountButton?.contains(event.target)) return;
    if (accountPopover) accountPopover.hidden = true;
    accountButton?.setAttribute("aria-expanded", "false");
});

document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape") return;
    if (accountPopover) accountPopover.hidden = true;
    accountButton?.setAttribute("aria-expanded", "false");
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
            window.sessionStorage.removeItem("parking_auth_token");
        }
    } catch {
        showAuthError("The backend could not be reached. Please try again.");
    }
}
