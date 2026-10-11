(function () {
  let historyData = [];
  let loading = false;

  const typeLabels = {
    text_to_morse: "Text → Morse",
    number_to_morse: "Number → Morse",
    morse_to_text: "Morse → Text",
    morse_to_number: "Morse → Number"
  };

  function getUserId() {
    return localStorage.getItem("user_id");
  }

  function formatDate(value) {
    if (!value) return "—";
    const date = new Date(value.replace(" ", "T") + (/[zZ]|[+-]\d\d:\d\d$/.test(value) ? "" : "Z"));
    return Number.isNaN(date.getTime()) ? value : date.toLocaleString();
  }

  function render() {
    const tableBody = document.getElementById("historyTableBody");
    const search = document.getElementById("historySearch");
    const filter = document.getElementById("historyFilter");
    if (!tableBody || !search || !filter) return;

    const query = search.value.trim().toLowerCase();
    const selectedType = filter.value;
    const rows = historyData.filter((row) => {
      const typeOK = selectedType === "all" || row.type === selectedType;
      const searchOK = [row.input, row.output, row.type, row.created_at]
        .some((value) => value.toLowerCase().includes(query));
      return typeOK && searchOK;
    });

    tableBody.replaceChildren();
    if (!rows.length) {
      const tr = document.createElement("tr");
      const cell = document.createElement("td");
      cell.colSpan = 4;
      cell.textContent = loading ? "Loading translation history…" : "No translation history found.";
      cell.style.textAlign = "center";
      tr.appendChild(cell);
      tableBody.appendChild(tr);
      return;
    }

    rows.forEach((row) => {
      const tr = document.createElement("tr");
      [row.input, row.type, row.output, formatDate(row.created_at)].forEach((value) => {
        const cell = document.createElement("td");
        cell.textContent = value;
        tr.appendChild(cell);
      });
      tableBody.appendChild(tr);
    });
  }

  async function loadHistory() {
    const tableBody = document.getElementById("historyTableBody");
    if (!tableBody) return;
    const userId = getUserId();
    if (!userId) {
      historyData = [];
      render();
      return;
    }

    loading = true;
    render();
    try {
      const response = await fetch(`${window.MORSE_BACKEND_URL}/history?user_id=${encodeURIComponent(userId)}`);
      if (!response.ok) throw new Error(`History request failed (${response.status})`);
      const data = await response.json();
      historyData = data.map((row) => ({
        input: String(row.input || ""),
        output: String(row.output || ""),
        type: typeLabels[row.type] || String(row.type || "Unknown"),
        created_at: String(row.created_at || "")
      }));
    } catch (error) {
      console.error("History loading error:", error);
      historyData = [];
    } finally {
      loading = false;
      render();
    }
  }

  async function clearHistory(triggerButton) {
    const userId = getUserId();
    if (!userId) return false;
    if (!window.confirm("Clear all saved translation history? This cannot be undone.")) return false;

    const buttons = [triggerButton, document.getElementById("clearHistoryBtn"), document.getElementById("settingsClearHistoryBtn")].filter(Boolean);
    buttons.forEach((button) => { button.disabled = true; });
    try {
      const response = await fetch(`${window.MORSE_BACKEND_URL}/clear-history?user_id=${encodeURIComponent(userId)}`);
      if (!response.ok) throw new Error(`Clear history failed (${response.status})`);
      historyData = [];
      render();
      if (typeof window.loadDashboard === "function") window.loadDashboard();
      if (typeof window.loadProfile === "function") window.loadProfile();
      return true;
    } catch (error) {
      console.error("Could not clear history:", error);
      window.alert("Translation history could not be cleared. Please try again.");
      return false;
    } finally {
      buttons.forEach((button) => { button.disabled = false; });
    }
  }

  function startHistory() {
    const search = document.getElementById("historySearch");
    const filter = document.getElementById("historyFilter");
    const clearButton = document.getElementById("clearHistoryBtn");
    if (!document.getElementById("historyTableBody")) return;
    if (search) search.addEventListener("input", render);
    if (filter) filter.addEventListener("change", render);
    if (clearButton) clearButton.addEventListener("click", (event) => clearHistory(event.currentTarget));
    window.loadHistory = loadHistory;
    window.clearHistory = clearHistory;
    loadHistory();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", startHistory, { once: true });
  } else {
    startHistory();
  }
})();
