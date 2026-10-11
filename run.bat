@echo off
setlocal

cd /d "%~dp0"

if not exist "%~dp0morse_server.exe" (
    echo morse_server.exe was not found in the project folder.
    pause
    exit /b 1
)

start "Morse Code Simulator Backend" "%~dp0morse_server.exe"
ping -n 2 127.0.0.1 >nul
start "" "%~dp0index.html"

endlocal
