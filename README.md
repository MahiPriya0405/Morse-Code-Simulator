# 📡 Morse Code Simulator

A Morse Code Simulator built with **C, HTML, CSS and JavaScript**. It lets users translate text to Morse code and back, practice with quizzes, look up the Morse alphabet, and track their activity, all through a clean web interface backed by a C server.

---

## ✨ Features

- 🔐 **Account access** – sign up and log in with a username or email
- 📊 **Dashboard** – overview of your activity and quick access to all modules
- 🔄 **Translator** – convert text → Morse code and Morse code → text
- 🧠 **Quiz** – test your Morse code knowledge
- 📖 **Reference** – full Morse code chart (A–Z, 0–9)
- 🕘 **History** – view your previous translations and activity
- 👤 **Profile** – manage your user details
- ⚙️ **Settings** – customize your preferences
- 🧭 **Navigation** – easy movement between all pages

---

## 🛠️ Tech Stack

| Layer    | Technology              |
| -------- | ----------------------- |
| Frontend | HTML5, CSS3, JavaScript |
| Backend  | C (custom server)       |
| Hosting  | GitHub Pages (frontend) |

---

## 📁 Project Structure

```
morse-code-simulator/
│
├── css/
│   ├── login.css
│   └── main.css
│
├── js/
│   ├── dashboard.js
│   ├── history.js
│   ├── login.js
│   ├── navigation.js
│   ├── profile.js
│   ├── quiz-ui.js
│   ├── reference.js
│   ├── settings.js
│   └── translator-ui.js
│
├── index.html          # Login page
├── main.html           # Main application page
├── morse_logic.c       # Morse encoding/decoding logic
├── morse_logic.h       # Header file for Morse logic
├── server.c            # C server
├── password_hash.c     # Password hashing for newly registered accounts
├── morse_server.exe    # Compiled server (Windows)
├── run.bat             # Starts the backend and opens the login page
└── README.md
```

---

## 🚀 Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/MahiPriya0405/Morse-Code-Simulator.git
cd Morse-Code-Simulator
```

### 2. Run the project on Windows

Double-click `run.bat`. It starts the C backend from the project folder and opens `index.html` in your browser. Keep the backend window open while using the app. Login, translation, and history need the backend running.

To start the parts manually, run `morse_server.exe` from this folder, then open `index.html` in your browser.

To rebuild the backend with MinGW GCC, run this from the project folder:

```bash
gcc server.c morse_logic.c database.c password_hash.c sqlite3.c -o morse_server.exe -lws2_32
```

---

## 📖 How It Works

1. The user logs in through `index.html`.
2. After login, `main.html` loads the dashboard and navigation.
3. In the **Translator**, text is converted to Morse code (dots `.` and dashes `-`) and vice versa.
4. The core conversion logic is written in C (`morse_logic.c`), and the frontend communicates with it through the C server.
5. Results are saved to **History**, and users can practice in the **Quiz** section.

### Example

| Text  | Morse Code             |
| ----- | ---------------------- |
| SOS   | `... --- ...`          |
| HELLO | `.... . .-.. .-.. ---` |

---

## 🔮 Future Improvements

- 🔊 Audio playback of Morse code (beeps)
- 💡 Flashing light visualization
- 🏆 Leaderboard for quiz scores
- 🌙 Dark / light theme toggle
- 📱 Better mobile responsiveness

---

## 👩‍💻 Author

**MahiPriya0405**
GitHub: [@MahiPriya0405](https://github.com/MahiPriya0405)

---

## 📄 License

This project is open source and available for learning and educational purposes.
