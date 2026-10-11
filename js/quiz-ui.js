const QUIZ_QUESTIONS = [
  { morse: "-.-.", answer: "C", options: ["C", "K", "R", "N"] },
  { morse: "....", answer: "H", options: ["S", "H", "B", "5"] },
  { morse: "--", answer: "M", options: ["M", "O", "T", "N"] },
  { morse: ".--.", answer: "P", options: ["J", "P", "X", "W"] },
  { morse: "...-", answer: "V", options: ["U", "F", "V", "3"] },
  { morse: "-----", answer: "0", options: ["9", "0", "O", "T"] },
  { morse: "..-.", answer: "F", options: ["F", "L", "6", "V"] }
];
 
const quizIntro = document.getElementById("quizIntro");
const quizActive = document.getElementById("quizActive");
const quizResult = document.getElementById("quizResult");
 
const startQuizBtn = document.getElementById("startQuizBtn");
const stopQuizBtn = document.getElementById("stopQuizBtn");
const restartQuizBtn = document.getElementById("restartQuizBtn");
 
const quizQuestion = document.getElementById("quizQuestion");
const quizOptions = document.getElementById("quizOptions");
const quizFeedback = document.getElementById("quizFeedback");
const quizTimerLine = document.getElementById("quizTimerLine");
const quizScoreLine = document.getElementById("quizScoreLine");
 
const QUESTION_TIME = 10;
let quizOrder = [];
let quizIndex = 0;
let quizScore = 0;
let quizCountdown = QUESTION_TIME;
let quizTimerId = null;
let transitionTimerId = null;
let answered = false;
 
function showQuizPanel(panel) {
  quizIntro.classList.add("hidden");
  quizActive.classList.add("hidden");
  quizResult.classList.add("hidden");
  panel.classList.remove("hidden");
}
 
function shuffle(array) {
  const copy = [...array];
  for (let i = copy.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [copy[i], copy[j]] = [copy[j], copy[i]];
  }
  return copy;
}
 
function startQuiz() {
  clearTimeout(transitionTimerId);
  document.getElementById("quizSaveStatus").textContent = "";
  quizOrder = shuffle(QUIZ_QUESTIONS);
  quizIndex = 0;
  quizScore = 0;
  showQuizPanel(quizActive);
  loadQuestion();
}
 
function loadQuestion() {
  answered = false;
  quizFeedback.textContent = "";
  const q = quizOrder[quizIndex];
  quizQuestion.innerHTML = `What does <code>${q.morse}</code> stand for?`;
  quizOptions.innerHTML = "";
 
  shuffle(q.options).forEach((option) => {
    const btn = document.createElement("button");
    btn.className = "quiz-option-btn";
    btn.textContent = option;
    btn.addEventListener("click", () => handleAnswer(btn, option, q.answer));
    quizOptions.appendChild(btn);
  });
 
  quizCountdown = QUESTION_TIME;
  quizTimerLine.textContent = `Time left: ${quizCountdown}s`;
  clearInterval(quizTimerId);
  quizTimerId = setInterval(() => {
    quizCountdown -= 1;
    quizTimerLine.textContent = `Time left: ${quizCountdown}s`;
    if (quizCountdown <= 0) {
      clearInterval(quizTimerId);
      if (!answered) {
        answered = true;
        quizFeedback.textContent = `Time's up. The answer was ${q.answer}.`;
        quizFeedback.style.color = "var(--danger)";
        transitionTimerId = setTimeout(nextQuestion, 1200);
      }
    }
  }, 1000);
}
 
function handleAnswer(button, chosen, correct) {
  if (answered) return;
  answered = true;
  clearInterval(quizTimerId);
 
  quizOptions.querySelectorAll(".quiz-option-btn").forEach((btn) => {
    if (btn.textContent === correct) {
      btn.classList.add("correct");
    } else if (btn === button) {
      btn.classList.add("incorrect");
    }
  });
 
  if (chosen === correct) {
    quizScore += 1;
    quizFeedback.textContent = "Correct.";
    quizFeedback.style.color = "var(--success)";
  } else {
    quizFeedback.textContent = `Not quite. The answer was ${correct}.`;
    quizFeedback.style.color = "var(--danger)";
  }
 
  transitionTimerId = setTimeout(nextQuestion, 1200);
}
 
function nextQuestion() {
  clearTimeout(transitionTimerId);
  transitionTimerId = null;
  quizIndex += 1;
  if (quizIndex >= quizOrder.length) {
    finishQuiz();
  } else {
    loadQuestion();
  }
}
 
function finishQuiz() {
  clearInterval(quizTimerId);
  quizScoreLine.textContent = `Score: ${quizScore} / ${quizOrder.length}`;
  showQuizPanel(quizResult);
  saveQuizScore();
}

async function saveQuizScore() {
  const status = document.getElementById("quizSaveStatus");
  const userId = localStorage.getItem("user_id");
  const score = Math.round((quizScore / quizOrder.length) * 100);
  if (!userId) {
    status.textContent = "Log in to save your quiz score.";
    return;
  }
  status.textContent = "Saving quiz score…";
  try {
    const query = new URLSearchParams({ user_id: userId, score: String(score) });
    const response = await fetch(`${window.MORSE_BACKEND_URL}/save-quiz?${query}`);
    if (!response.ok) throw new Error(`Score save failed (${response.status})`);
    status.textContent = `Score saved: ${score}%.`;
    if (typeof window.loadDashboard === "function") window.loadDashboard();
    if (typeof window.loadProfile === "function") window.loadProfile();
  } catch (error) {
    console.error("Quiz score save error:", error);
    status.textContent = "Quiz finished, but the score could not be saved.";
  }
}
 
function stopQuiz() {
  clearInterval(quizTimerId);
  clearTimeout(transitionTimerId);
  transitionTimerId = null;
  showQuizPanel(quizIntro);
}
 
startQuizBtn.addEventListener("click", startQuiz);
stopQuizBtn.addEventListener("click", stopQuiz);
restartQuizBtn.addEventListener("click", startQuiz);
 
