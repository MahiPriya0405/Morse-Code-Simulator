const loginForm = document.getElementById("loginForm");

const loginMessage = document.getElementById("loginMessage");

const togglePasswordBtn =
    document.getElementById("togglePassword");

const passwordInput =
    document.getElementById("loginPassword");

const forgotPasswordLink =
    document.getElementById("forgotPasswordLink");


/* =========================================================
   BACKEND SERVER
   ========================================================= */

const BACKEND_URL =
    "http://127.0.0.1:8080";


/* =========================================================
   SHOW / HIDE PASSWORD
   ========================================================= */

togglePasswordBtn.addEventListener("click", () => {

    const isHidden =
        passwordInput.type === "password";

    passwordInput.type =
        isHidden ? "text" : "password";

    togglePasswordBtn.setAttribute(
        "aria-label",
        isHidden
            ? "Hide password"
            : "Show password"
    );

    togglePasswordBtn.textContent =
        isHidden ? "\u{1F576}" : "\u{1F441}";
});


/* =========================================================
   LOGIN FORM
   ========================================================= */

loginForm.addEventListener(
    "submit",
    async (e) => {

        e.preventDefault();


        /* Get username/email */

        const identifier =
            document
                .getElementById("loginIdentifier")
                .value
                .trim();


        /* Get password */

        const password =
            passwordInput.value;


        /* Check empty fields */

        if (!identifier || !password) {

            loginMessage.textContent =
                "Please fill in both fields.";

            loginMessage.className =
                "form-message error";

            return;
        }


        /* Show logging in message */

        loginMessage.textContent =
            "Logging in...";

        loginMessage.className =
            "form-message success";


        try {

            /* =================================================
               SEND LOGIN DETAILS TO C SERVER
               ================================================= */

            const url =
                BACKEND_URL +
                "/login" +
                "?identifier=" +
                encodeURIComponent(identifier) +
                "&password=" +
                encodeURIComponent(password);


            const response =
                await fetch(url);


            /* Get response from C server */

            const result =
                await response.text();


            console.log(
                "Server response:",
                result
            );


            /* =================================================
               SUCCESSFUL LOGIN
               ================================================= */

            const match = result.match(/^LOGIN_SUCCESS:(\d+)$/);
            if (response.ok && match) {

                /*
                   Example server response:

                   LOGIN_SUCCESS:1

                   Here 1 is the user's database ID.
                */

                const userId = match[1];


                /* Save user ID in browser */

                localStorage.setItem(
                    "user_id",
                    userId
                );


                console.log(
                    "Logged in user ID:",
                    userId
                );


                /* Show success message */

                loginMessage.textContent =
                    "Login successful!";

                loginMessage.className =
                    "form-message success";


                /* Open main page */

                setTimeout(() => {

                    window.location.href =
                        "main.html?user_id=" +
                        encodeURIComponent(userId);

                }, 500);

            }


            /* =================================================
               LOGIN FAILED
               ================================================= */

            else {

                loginMessage.textContent =
                    "Invalid username/email or password.";

                loginMessage.className =
                    "form-message error";
            }

        }


        /* =====================================================
           BACKEND CONNECTION ERROR
           ===================================================== */

        catch (error) {

            console.error(
                "Login error:",
                error
            );


            loginMessage.textContent =
                "Cannot connect to the backend server.";

            loginMessage.className =
                "form-message error";
        }

    }
);


/* =========================================================
   FORGOT PASSWORD
   ========================================================= */

forgotPasswordLink.addEventListener(
    "click",
    (e) => {

        e.preventDefault();


        loginMessage.textContent =
            "Password reset isn't connected yet.";

        loginMessage.className =
            "form-message error";
    }
);


/* =========================================================
   SIGNUP FORM
   ========================================================= */

const signupForm = document.getElementById("signupForm");
const signupMessage = document.getElementById("signupMessage");
const signupSubmit = document.getElementById("signupSubmit");
const signupPrompt = document.getElementById("showSignupPrompt");

function showSignupMessage(message, type = "error") {
    signupMessage.textContent = message;
    signupMessage.className = `form-message ${type}`;
}

function showLoginMessage(message, type = "success") {
    loginMessage.textContent = message;
    loginMessage.className = `form-message ${type}`;
}

function openSignup() {
    loginMessage.textContent = "";
    loginForm.classList.add("hidden");
    signupPrompt.classList.add("hidden");
    signupForm.classList.remove("hidden");
    signupMessage.textContent = "";
    document.getElementById("signupName").focus();
}

function openLogin(message = "") {
    signupForm.classList.add("hidden");
    loginForm.classList.remove("hidden");
    signupPrompt.classList.remove("hidden");
    if (message) showLoginMessage(message);
    else loginMessage.textContent = "";
    document.getElementById("loginIdentifier").focus();
}

document.getElementById("showSignup").addEventListener("click", (event) => {
    event.preventDefault();
    openSignup();
});

document.getElementById("showLogin").addEventListener("click", (event) => {
    event.preventDefault();
    openLogin();
});

signupForm.addEventListener("submit", async (event) => {
    event.preventDefault();

    const name = document.getElementById("signupName").value.trim();
    const username = document.getElementById("signupUsername").value.trim();
    const email = document.getElementById("signupEmail").value.trim();
    const password = document.getElementById("signupPassword").value;
    const confirmPassword = document.getElementById("signupConfirmPassword").value;

    if (!name) {
        showSignupMessage("Enter your name.");
        document.getElementById("signupName").focus();
        return;
    }
    if (name.length > 255) {
        showSignupMessage("Name must be 255 characters or fewer.");
        document.getElementById("signupName").focus();
        return;
    }
    if (!username) {
        showSignupMessage("Choose a username.");
        document.getElementById("signupUsername").focus();
        return;
    }
    if (username.length > 50 || /\s/.test(username)) {
        showSignupMessage("Username must be at most 50 characters and contain no spaces.");
        document.getElementById("signupUsername").focus();
        return;
    }
    if (!email) {
        showSignupMessage("Enter your email address.");
        document.getElementById("signupEmail").focus();
        return;
    }
    if (email.length > 254 || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
        showSignupMessage("Enter a valid email address.");
        document.getElementById("signupEmail").focus();
        return;
    }
    if (!password) {
        showSignupMessage("Create a password.");
        document.getElementById("signupPassword").focus();
        return;
    }
    if (password.length > 255) {
        showSignupMessage("Password must be 255 characters or fewer.");
        document.getElementById("signupPassword").focus();
        return;
    }
    if (!confirmPassword) {
        showSignupMessage("Confirm your password.");
        document.getElementById("signupConfirmPassword").focus();
        return;
    }
    if (password !== confirmPassword) {
        showSignupMessage("Passwords do not match.");
        document.getElementById("signupConfirmPassword").focus();
        return;
    }

    signupSubmit.disabled = true;
    showSignupMessage("Creating your account...", "success");

    try {
        const response = await fetch(`${BACKEND_URL}/signup`, {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
            },
            body: new URLSearchParams({ name, username, email, password })
        });
        const result = await response.json().catch(() => ({}));

        if (response.status === 201 && result.ok) {
            signupForm.reset();
            openLogin("Account created successfully. Please log in.");
        } else {
            showSignupMessage(result.error || "The account could not be created.");
        }
    } catch (error) {
        showSignupMessage("Cannot connect to the backend server.");
    } finally {
        signupSubmit.disabled = false;
    }
});
