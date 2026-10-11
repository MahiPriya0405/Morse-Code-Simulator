const REFERENCE_DATA = [
  { char: "A", code: ".-" }, { char: "B", code: "-..." }, { char: "C", code: "-.-." },
  { char: "D", code: "-.." }, { char: "E", code: "." }, { char: "F", code: "..-." },
  { char: "G", code: "--." }, { char: "H", code: "...." }, { char: "I", code: ".." },
  { char: "J", code: ".---" }, { char: "K", code: "-.-" }, { char: "L", code: ".-.." },
  { char: "M", code: "--" }, { char: "N", code: "-." }, { char: "O", code: "---" },
  { char: "P", code: ".--." }, { char: "Q", code: "--.-" }, { char: "R", code: ".-." },
  { char: "S", code: "..." }, { char: "T", code: "-" }, { char: "U", code: "..-" },
  { char: "V", code: "...-" }, { char: "W", code: ".--" }, { char: "X", code: "-..-" },
  { char: "Y", code: "-.--" }, { char: "Z", code: "--.." },
  { char: "0", code: "-----" }, { char: "1", code: ".----" }, { char: "2", code: "..---" },
  { char: "3", code: "...--" }, { char: "4", code: "....-" }, { char: "5", code: "....." },
  { char: "6", code: "-...." }, { char: "7", code: "--..." }, { char: "8", code: "---.." },
  { char: "9", code: "----." }
];
 
const referenceGrid = document.getElementById("referenceGrid");
const referenceSearch = document.getElementById("referenceSearch");
 
function renderReferenceGrid(filterText = "") {
  const query = filterText.trim().toUpperCase();
  referenceGrid.innerHTML = "";
 
  REFERENCE_DATA
    .filter((entry) => entry.char.includes(query) || entry.code.includes(query))
    .forEach((entry) => {
      const chip = document.createElement("div");
      chip.className = "reference-chip";
      chip.innerHTML = `<span class="ref-char">${entry.char}</span><span class="ref-code">${entry.code}</span>`;
      referenceGrid.appendChild(chip);
    });
}
 
referenceSearch.addEventListener("input", () => renderReferenceGrid(referenceSearch.value));
 
renderReferenceGrid();
 