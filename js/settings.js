(function () {
  const accountMessage = document.getElementById("settingsAccountMessage");
  const passwordMessage = document.getElementById("settingsPasswordMessage");

  function displayMessage(element, text, success) {
    if (!element) return;
    element.textContent = text;
    element.className = `form-message ${success ? "success" : "error"}`;
  }

  async function loadSettings() {
    const userId = localStorage.getItem("user_id");
    if (!userId) return;
    try {
      const response = await fetch(`${window.MORSE_BACKEND_URL}/profile?user_id=${encodeURIComponent(userId)}`);
      if (!response.ok) throw new Error(`Account request failed (${response.status})`);
      const profile = await response.json();
      document.getElementById("settingsUsername").value = profile.username || "";
      document.getElementById("settingsEmail").value = profile.email || "";
    } catch (error) {
      console.error("Settings loading error:", error);
      displayMessage(accountMessage, "Account details could not be loaded.", false);
    }
  }

  async function updateAccount() {
    const userId = localStorage.getItem("user_id");
    const username = document.getElementById("settingsUsername").value.trim();
    const email = document.getElementById("settingsEmail").value.trim();
    if (!userId || !username || !email) {
      displayMessage(accountMessage, "Enter both a username and email address.", false);
      return;
    }
    const button = document.getElementById("saveAccountChangesBtn");
    button.disabled = true;
    try {
      const params = new URLSearchParams({ user_id: userId, username, email });
      const response = await fetch(`${window.MORSE_BACKEND_URL}/update-account?${params}`);
      if (!response.ok) {
        const detail = await response.json().catch(() => ({}));
        throw new Error(detail.error || `Account update failed (${response.status})`);
      }
      displayMessage(accountMessage, "Account details updated.", true);
      if (typeof window.loadProfile === "function") window.loadProfile();
    } catch (error) {
      displayMessage(accountMessage, error.message.includes("already in use") ? "That username or email is already in use." : error.message, false);
    } finally {
      button.disabled = false;
    }
  }

  async function updatePassword() {
    const userId = localStorage.getItem("user_id");
    const password = document.getElementById("settingsNewPassword").value;
    const confirmation = document.getElementById("settingsConfirmPassword").value;
    if (!userId || !password || !confirmation) {
      displayMessage(passwordMessage, "Enter and confirm your new password.", false);
      return;
    }
    if (password !== confirmation) {
      displayMessage(passwordMessage, "The passwords do not match.", false);
      return;
    }
    const button = document.getElementById("updatePasswordBtn");
    button.disabled = true;
    try {
      const params = new URLSearchParams({ user_id: userId, password });
      const response = await fetch(`${window.MORSE_BACKEND_URL}/change-password?${params}`);
      if (!response.ok) {
        const detail = await response.json().catch(() => ({}));
        throw new Error(detail.error || `Password update failed (${response.status})`);
      }
      document.getElementById("settingsNewPassword").value = "";
      document.getElementById("settingsConfirmPassword").value = "";
      displayMessage(passwordMessage, "Password updated.", true);
    } catch (error) {
      displayMessage(passwordMessage, error.message, false);
    } finally {
      button.disabled = false;
    }
  }

  const darkModeToggle = document.getElementById("darkModeToggle");
  const soundToggle = document.getElementById("soundToggle");
  const notificationsToggle = document.getElementById("notificationsToggle");
  const preferenceControls = [
    [darkModeToggle, "morse_dark_mode", (checked) => document.documentElement.setAttribute("data-theme", checked ? "dark" : "light")],
    [soundToggle, "morse_sound_enabled"],
    [notificationsToggle, "morse_quiz_notifications"]
  ];
  preferenceControls.forEach(([control, key, apply]) => {
    if (!control) return;
    const savedValue = localStorage.getItem(key);
    if (savedValue !== null) control.checked = savedValue === "true";
    if (apply) apply(control.checked);
    control.addEventListener("change", () => {
      localStorage.setItem(key, String(control.checked));
      if (apply) apply(control.checked);
    });
  });

  document.getElementById("saveAccountChangesBtn").addEventListener("click", updateAccount);
  document.getElementById("updatePasswordBtn").addEventListener("click", updatePassword);
  document.getElementById("settingsClearHistoryBtn").addEventListener("click", (event) => {
    if (typeof window.clearHistory === "function") window.clearHistory(event.currentTarget);
  });
  document.getElementById("settingsLogoutBtn").addEventListener("click", () => {
    if (typeof window.logout === "function") window.logout();
  });

  window.loadSettings = loadSettings;
  loadSettings();
})();
