(function () {
  const typeLabels = {
    text_to_morse: "Text → Morse",
    number_to_morse: "Number → Morse",
    morse_to_text: "Morse → Text",
    morse_to_number: "Morse → Number"
  };

  function addText(parent, tag, className, value) {
    const element = document.createElement(tag);
    if (className) element.className = className;
    element.textContent = value;
    parent.appendChild(element);
    return element;
  }

  function formatTime(value) {
    if (!value) return "";
    const date = new Date(value.replace(" ", "T") + (/[zZ]|[+-]\d\d:\d\d$/.test(value) ? "" : "Z"));
    return Number.isNaN(date.getTime()) ? value : date.toLocaleString();
  }

  function render(data) {
    const statGrid = document.getElementById("statGrid");
    const chart = document.getElementById("activityChart");
    const recentList = document.getElementById("recentActivityList");
    const usage = document.getElementById("usageBars");
    const quizStats = document.getElementById("quizStatsSummary");
    const message = document.getElementById("dashboardMessage");
    if (!statGrid || !chart || !recentList || !usage || !quizStats) return;

    const statItems = [
      ["↔", "Total translations", data.totalTranslations],
      ["A", "Text → Morse", data.textToMorse],
      ["123", "Number → Morse", data.numberToMorse],
      ["·−", "Morse → Text", data.morseToText],
      ["−·", "Morse → Number", data.morseToNumber]
    ];
    statGrid.replaceChildren();
    statItems.forEach(([icon, label, value]) => {
      const card = document.createElement("div");
      card.className = "stat-card";
      addText(card, "span", "stat-icon", icon);
      addText(card, "span", "stat-value", String(value || 0));
      addText(card, "span", "stat-label", label);
      statGrid.appendChild(card);
    });

    const counts = new Map((data.dailyActivity || []).map((item) => [item.date, Number(item.count) || 0]));
    const days = [];
    const today = new Date();
    for (let offset = 6; offset >= 0; offset--) {
      const date = new Date(today.getFullYear(), today.getMonth(), today.getDate() - offset);
      const key = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
      days.push({ key, date, count: counts.get(key) || 0 });
    }
    const maxCount = Math.max(1, ...days.map((day) => day.count));
    chart.replaceChildren();
    days.forEach((day) => {
      const column = document.createElement("div");
      column.className = "bar-col";
      const bar = document.createElement("div");
      bar.className = "bar";
      bar.style.height = `${day.count ? Math.max(4, (day.count / maxCount) * 88) : 0}px`;
      bar.title = `${day.count} translation${day.count === 1 ? "" : "s"}`;
      column.appendChild(bar);
      addText(column, "span", "bar-label", day.date.toLocaleDateString(undefined, { weekday: "short" }));
      chart.appendChild(column);
    });

    recentList.replaceChildren();
    const recentRows = data.recentActivity || [];
    if (!recentRows.length) {
      addText(recentList, "li", "", "No translations yet.");
    } else {
      recentRows.forEach((row) => {
        const item = document.createElement("li");
        const details = document.createElement("div");
        addText(details, "span", "activity-main", row.input || "");
        addText(details, "span", "activity-type", typeLabels[row.type] || row.type || "Translation");
        item.appendChild(details);
        addText(item, "span", "activity-time", formatTime(row.created_at));
        recentList.appendChild(item);
      });
    }

    const usageItems = [
      ["Text → Morse", Number(data.textToMorse) || 0],
      ["Number → Morse", Number(data.numberToMorse) || 0],
      ["Morse → Text", Number(data.morseToText) || 0],
      ["Morse → Number", Number(data.morseToNumber) || 0]
    ];
    const total = Number(data.totalTranslations) || 0;
    usage.replaceChildren();
    usageItems.forEach(([label, count]) => {
      const row = document.createElement("div");
      row.className = "usage-bar-row";
      addText(row, "span", "", label);
      const track = document.createElement("div");
      track.className = "usage-bar-track";
      const fill = document.createElement("div");
      fill.className = "usage-bar-fill";
      fill.style.width = `${total ? Math.round((count / total) * 100) : 0}%`;
      track.appendChild(fill);
      row.appendChild(track);
      addText(row, "strong", "", String(count));
      usage.appendChild(row);
    });

    quizStats.replaceChildren();
    [[data.quizAttempts, "Attempts"], [`${data.bestQuizScore || 0}%`, "Best score"], [`${data.latestQuizScore || 0}%`, "Latest score"]]
      .forEach(([value, label]) => {
        const item = document.createElement("div");
        addText(item, "span", "", String(value || 0));
        addText(item, "small", "", label);
        quizStats.appendChild(item);
      });

    if (message) {
      message.textContent = total ? "Dashboard data is up to date." : "Start translating to see your activity here.";
      message.className = "dummy-note";
    }
  }

  async function loadDashboard() {
    const message = document.getElementById("dashboardMessage");
    const userId = localStorage.getItem("user_id");
    if (!userId) {
      if (message) message.textContent = "Please log in to view your dashboard.";
      return;
    }
    if (message) message.textContent = "Loading dashboard data…";
    try {
      const response = await fetch(`${window.MORSE_BACKEND_URL}/dashboard?user_id=${encodeURIComponent(userId)}`);
      if (!response.ok) throw new Error(`Dashboard request failed (${response.status})`);
      render(await response.json());
    } catch (error) {
      console.error("Dashboard loading error:", error);
      if (message) message.textContent = "Dashboard data could not be loaded. Check that the backend is running.";
    }
  }

  window.loadDashboard = loadDashboard;
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", loadDashboard, { once: true });
  } else {
    loadDashboard();
  }
})();
