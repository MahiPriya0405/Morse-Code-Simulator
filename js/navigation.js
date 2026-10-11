const navItems = document.querySelectorAll(".nav-item[data-page]");
const pages = document.querySelectorAll(".page");
const pageTitle = document.getElementById("pageTitle");
window.MORSE_BACKEND_URL = window.MORSE_BACKEND_URL || "http://127.0.0.1:8080";

const incomingUserId = new URLSearchParams(window.location.search).get("user_id");
if (incomingUserId && /^[1-9]\d*$/.test(incomingUserId)) {
    localStorage.setItem("user_id", incomingUserId);
    try {
        window.history.replaceState(null, "", window.location.pathname + window.location.hash);
    } catch (error) {
        console.warn("Could not remove the login handoff from the address bar.", error);
    }
}

if (!localStorage.getItem("user_id")) {
    window.location.replace("index.html");
}

const PAGE_TITLES = {
    home: "Home",
    dashboard: "Dashboard",
    translator: "Morse Translator",
    reference: "Morse Code Overview",
    history: "Translation History",
    quiz: "Quiz",
    profile: "Profile",
    settings: "Settings"
};


/* =========================================================
   CHANGE PAGE
   ========================================================= */

function goToPage(pageId) {

    console.log("Navigation requested:", pageId);

    const target =
        document.getElementById("page-" + pageId);

    if (!target) {
        console.error("Page not found:", pageId);
        return;
    }

    pages.forEach(function(page) {
        page.classList.remove("active");
    });

    navItems.forEach(function(item) {
        item.classList.remove("active");
    });

    target.classList.add("active");

    const navButton =
        document.querySelector(
            '.nav-item[data-page="' + pageId + '"]'
        );

    if (navButton) {
        navButton.classList.add("active");
    }

    if (pageTitle) {
        pageTitle.textContent =
            PAGE_TITLES[pageId] || "";
    }

    window.scrollTo(0, 0);

    if (pageId === "dashboard" && typeof window.loadDashboard === "function") {
        window.loadDashboard();
    } else if (pageId === "history" && typeof window.loadHistory === "function") {
        window.loadHistory();
    } else if (pageId === "profile" && typeof window.loadProfile === "function") {
        window.loadProfile();
    } else if (pageId === "settings" && typeof window.loadSettings === "function") {
        window.loadSettings();
    }
}


/* =========================================================
   SIDEBAR NAVIGATION
   ========================================================= */

navItems.forEach(function(button) {

    button.addEventListener("click", function(event) {

        event.preventDefault();
        event.stopPropagation();

        const pageId =
            button.getAttribute("data-page");

        console.log(
            "Sidebar clicked:",
            pageId
        );

        goToPage(pageId);

    });

});


/* =========================================================
   START TRANSLATING BUTTON
   ========================================================= */

document
    .querySelectorAll("[data-goto]")
    .forEach(function(button) {

        button.addEventListener("click", function(event) {

            event.preventDefault();
            event.stopPropagation();

            const pageId =
                button.getAttribute("data-goto");

            goToPage(pageId);

        });

    });


/* =========================================================
   MOBILE SIDEBAR
   ========================================================= */

const sidebar =
    document.getElementById("sidebar");

const sidebarOverlay =
    document.getElementById("sidebarOverlay");

const hamburgerBtn =
    document.getElementById("hamburgerBtn");


function openSidebar() {

    if (sidebar) {
        sidebar.classList.add("open");
    }

    if (sidebarOverlay) {
        sidebarOverlay.classList.add("open");
    }

}


function closeSidebar() {

    if (sidebar) {
        sidebar.classList.remove("open");
    }

    if (sidebarOverlay) {
        sidebarOverlay.classList.remove("open");
    }

}


if (hamburgerBtn) {

    hamburgerBtn.addEventListener(
        "click",
        function(event) {

            event.preventDefault();
            event.stopPropagation();

            openSidebar();

        }
    );

}


if (sidebarOverlay) {

    sidebarOverlay.addEventListener(
        "click",
        function(event) {

            event.preventDefault();

            closeSidebar();

        }
    );

}


/* =========================================================
   LOGOUT
   ========================================================= */

function logout() {
    localStorage.removeItem("user_id");
    window.location.href = "index.html";
}
window.logout = logout;

const logoutBtn =
    document.getElementById("logoutBtn");


if (logoutBtn) {

    logoutBtn.addEventListener(
        "click",
        function(event) {

            event.preventDefault();
            event.stopPropagation();

            logout();

        }
    );

}
