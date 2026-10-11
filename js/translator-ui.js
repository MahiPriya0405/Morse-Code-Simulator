const BACKEND_URL = window.MORSE_BACKEND_URL || "http://127.0.0.1:8080";

/* =========================================================
   BACKEND CALL
   ========================================================= */

function callBackend(endpoint, input) {

  const userId = localStorage.getItem("user_id");

  console.log("Sending translation:");
  console.log("Endpoint:", endpoint);
  console.log("Input:", input);
  console.log("User ID:", userId);

  if (!userId) {
    return Promise.reject(
      new Error("User ID not found. Please login again.")
    );
  }

  const url =
    `${BACKEND_URL}/${endpoint}` +
    `?input=${encodeURIComponent(input)}` +
    `&user_id=${encodeURIComponent(userId)}`;

  console.log("Backend URL:", url);

  return fetch(url)
    .then((response) => {

      if (!response.ok) {
        throw new Error(
          "Backend returned HTTP " + response.status
        );
      }

      return response.text();
    });
}


/* =========================================================
   BACKEND CONNECTION STATUS
   ========================================================= */

const backendStatus =
  document.getElementById("backendStatus");

function checkBackendConnection() {

  if (!backendStatus) {
    return;
  }

  const userId = localStorage.getItem("user_id");
  if (!userId) {
    backendStatus.textContent = "Log in to connect to the backend.";
    backendStatus.className = "backend-status offline";
    return;
  }

  fetch(`${BACKEND_URL}/profile?user_id=${encodeURIComponent(userId)}`)
    .then((response) => {

      if (!response.ok) {
        throw new Error("Backend error");
      }

      backendStatus.textContent =
        "Connected to backend (127.0.0.1:8080)";

      backendStatus.className =
        "backend-status online";
    })
    .catch(() => {

      backendStatus.textContent =
        "Backend not reachable. Make sure morse_server is running.";

      backendStatus.className =
        "backend-status offline";
    });
}

checkBackendConnection();


/* =========================================================
   SETUP ONE TRANSLATOR
   ========================================================= */

function setupConverter({
  inputId,
  outputId,
  enterId,
  resetId,
  copyId,
  counterId,
  endpoint,
  emptyMessage,
  placeholder
}) {

  const input =
    document.getElementById(inputId);

  const output =
    document.getElementById(outputId);

  const enterBtn =
    document.getElementById(enterId);

  const resetBtn =
    document.getElementById(resetId);

  const copyBtn =
    document.getElementById(copyId);

  const counter =
    document.getElementById(counterId);
  let converting = false;


  /* Safety check */

  if (
    !input ||
    !output ||
    !enterBtn ||
    !resetBtn ||
    !copyBtn ||
    !counter
  ) {

    console.error(
      "Translator element missing:",
      {
        inputId,
        outputId,
        enterId,
        resetId,
        copyId,
        counterId
      }
    );

    return;
  }


  /* =======================================================
     CHARACTER COUNTER
     ======================================================= */

  input.addEventListener("input", () => {

    counter.textContent =
      input.value.length;

  });


  /* =======================================================
     ENTER BUTTON
     ======================================================= */

  enterBtn.addEventListener("click", async (event) => {

    /*
      IMPORTANT:
      Prevent navigation / form submission.
    */

    event.preventDefault();
    event.stopPropagation();
    event.stopImmediatePropagation();

    if (converting) return;


    console.log(
      "Translator Enter clicked:",
      enterId
    );


    const value =
      input.value.trim();


    /* Empty input */

    if (!value) {

      output.textContent =
        emptyMessage;

      return;
    }


    /* Show loading */

    output.textContent =
      "Converting...";
    converting = true;
    enterBtn.disabled = true;


    try {

      /* Call C backend */

      const result =
        await callBackend(
          endpoint,
          value
        );


      /* Display result */

      output.textContent =
        result;


      console.log(
        "Translation successful:",
        result
      );


      /* =================================================
         REFRESH HISTORY
         ================================================= */

      if (
        typeof window.loadHistory === "function"
      ) {

        console.log(
          "Refreshing translation history..."
        );

        window.loadHistory();
      }

      if (typeof window.loadDashboard === "function") window.loadDashboard();
      if (typeof window.loadProfile === "function") window.loadProfile();


    } catch (error) {

      console.error(
        "Translation error:",
        error
      );

      output.textContent =
        "Translation failed. Make sure the backend is running.";

    } finally {
      converting = false;
      enterBtn.disabled = false;

    }

  });


  /* =======================================================
     RESET BUTTON
     ======================================================= */

  resetBtn.addEventListener("click", (event) => {

    event.preventDefault();
    event.stopPropagation();

    input.value = "";

    counter.textContent =
      "0";

    output.textContent =
      placeholder;

  });


  /* =======================================================
     COPY BUTTON
     ======================================================= */

  copyBtn.addEventListener("click", (event) => {

    event.preventDefault();
    event.stopPropagation();

    const text =
      output.textContent;


    if (
      !text ||
      text === placeholder ||
      text === emptyMessage
    ) {

      return;
    }


    navigator.clipboard
      .writeText(text)
      .then(() => {

        const original =
          copyBtn.innerHTML;

        copyBtn.textContent =
          "✓";

        setTimeout(() => {

          copyBtn.innerHTML =
            original;

        }, 1000);

      })
      .catch((error) => {

        console.error(
          "Copy failed:",
          error
        );

      });

  });

}


/* =========================================================
   TEXT → MORSE
   ========================================================= */

setupConverter({

  inputId: "textInput",

  outputId: "textOutput",

  enterId: "textEnter",

  resetId: "textReset",

  copyId: "textCopy",

  counterId: "textCharCount",

  endpoint: "text-to-morse",

  emptyMessage:
    "Type something first",

  placeholder:
    "Morse output will appear here"

});


/* =========================================================
   NUMBER → MORSE
   ========================================================= */

setupConverter({

  inputId: "numberInput",

  outputId: "numberOutput",

  enterId: "numberEnter",

  resetId: "numberReset",

  copyId: "numberCopy",

  counterId: "numberCharCount",

  endpoint: "number-to-morse",

  emptyMessage:
    "Type a number first",

  placeholder:
    "Morse output will appear here"

});


/* =========================================================
   MORSE → TEXT
   ========================================================= */

setupConverter({

  inputId: "morseTextInput",

  outputId: "morseTextOutput",

  enterId: "morseTextEnter",

  resetId: "morseTextReset",

  copyId: "morseTextCopy",

  counterId: "morseTextCharCount",

  endpoint: "morse-to-text",

  emptyMessage:
    "Type morse code first",

  placeholder:
    "Text output will appear here"

});


/* =========================================================
   MORSE → NUMBER
   ========================================================= */

setupConverter({

  inputId: "morseNumberInput",

  outputId: "morseNumberOutput",

  enterId: "morseNumberEnter",

  resetId: "morseNumberReset",

  copyId: "morseNumberCopy",

  counterId: "morseNumberCharCount",

  endpoint: "morse-to-number",

  emptyMessage:
    "Type morse code first",

  placeholder:
    "Number output will appear here"

});
