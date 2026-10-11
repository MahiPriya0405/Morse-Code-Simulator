(function () {
  const editProfileBtn = document.getElementById("editProfileBtn");
  const editProfileForm = document.getElementById("editProfileForm");
  const cancelEditBtn = document.getElementById("cancelEditBtn");
  const message = document.getElementById("profileMessage");

  function showMessage(text, success) {
    if (!message) return;
    message.textContent = text;
    message.className = `form-message ${success ? "success" : "error"}`;
  }

  function fillProfile(profile) {
    const name = profile.name || profile.username || "User";
    document.getElementById("profileName").textContent = name;
    document.getElementById("profileUsername").textContent = `@${profile.username || ""}`;
    document.getElementById("profileEmail").textContent = profile.email || "";
    document.getElementById("profileTranslationCount").textContent = String(profile.totalTranslations || 0);
    document.getElementById("profileQuizAttempts").textContent = String(profile.quizAttempts || 0);
    document.getElementById("profileBestQuizScore").textContent = `${profile.bestQuizScore || 0}%`;
    const avatar = document.querySelector(".profile-avatar");
    if (avatar) avatar.textContent = name.trim().charAt(0).toUpperCase() || "M";
    document.getElementById("editName").value = name;
    document.getElementById("editUsername").value = profile.username || "";
    document.getElementById("editEmail").value = profile.email || "";
  }

  async function loadProfile() {
    const userId = localStorage.getItem("user_id");
    if (!userId) return;
    try {
      const response = await fetch(`${window.MORSE_BACKEND_URL}/profile?user_id=${encodeURIComponent(userId)}`);
      if (!response.ok) throw new Error(`Profile request failed (${response.status})`);
      const profile = await response.json();
      fillProfile(profile);
    } catch (error) {
      console.error("Profile loading error:", error);
      showMessage("Profile could not be loaded. Check that the backend is running.", false);
    }
  }

  if (editProfileBtn && editProfileForm) {
    editProfileBtn.addEventListener("click", () => {
      editProfileForm.classList.remove("hidden");
      editProfileBtn.classList.add("hidden");
      if (message) message.textContent = "";
    });
    if (cancelEditBtn) cancelEditBtn.addEventListener("click", () => {
      editProfileForm.classList.add("hidden");
      editProfileBtn.classList.remove("hidden");
      if (message) message.textContent = "";
    });

    editProfileForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const userId = localStorage.getItem("user_id");
      const fields = {
        name: document.getElementById("editName").value.trim(),
        username: document.getElementById("editUsername").value.trim(),
        email: document.getElementById("editEmail").value.trim()
      };
      if (!userId || Object.values(fields).some((value) => !value)) {
        showMessage("Please complete all profile fields.", false);
        return;
      }

      const submitButton = editProfileForm.querySelector('[type="submit"]');
      if (submitButton) submitButton.disabled = true;
      try {
        const params = new URLSearchParams({ user_id: userId, ...fields });
        const response = await fetch(`${window.MORSE_BACKEND_URL}/update-profile?${params}`);
        if (!response.ok) {
          const detail = await response.json().catch(() => ({}));
          throw new Error(detail.error || `Profile update failed (${response.status})`);
        }
        await loadProfile();
        editProfileForm.classList.add("hidden");
        editProfileBtn.classList.remove("hidden");
        showMessage("Profile updated.", true);
        if (typeof window.loadSettings === "function") window.loadSettings();
      } catch (error) {
        showMessage(error.message.includes("already in use") ? "That username or email is already in use." : error.message, false);
      } finally {
        if (submitButton) submitButton.disabled = false;
      }
    });
  }

  window.loadProfile = loadProfile;
  loadProfile();
})();
