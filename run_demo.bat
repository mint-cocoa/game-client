@echo off
REM ============================================================
REM  Demo recording launcher
REM  - Starts the ServerCore game server in a WSL terminal
REM  - Launches two client instances side-by-side (960x540 each)
REM  - Each client's working directory is set to run\P1 or run\P2
REM    so events_<pid>.jsonl + imgui.ini are kept per-instance
REM ============================================================

setlocal

set EXE=%~dp0x64\Debug\IsometricClient.exe
if not exist "%EXE%" (
    echo [ERROR] Client exe not found: %EXE%
    echo Build the Debug x64 configuration first.
    pause
    exit /b 1
)

REM --- Separate working directories so events_*.jsonl + imgui.ini don't clash ---
set DIR_A=%~dp0run\P1
set DIR_B=%~dp0run\P2
if not exist "%DIR_A%" mkdir "%DIR_A%"
if not exist "%DIR_B%" mkdir "%DIR_B%"

REM --- Start the server in WSL (new Windows Terminal tab) ---
REM Adjust the path below if your build directory differs.
start "" wt -w 0 new-tab --title "GameServer" wsl -d Ubuntu-24.04 -- bash -lc ^
 "cd ~/servercore_v4/build && ./examples/GameServer/GameServer 2>&1 | tee ~/gameserver.log"

REM Give the server a moment to bind the port.
timeout /t 2 /nobreak >nul

REM --- Launch Client A (top-left, "Client P1") ---
start "" /D "%DIR_A%" "%EXE%" -t "Client P1" -w 960 -h 540 -x 0 -y 0

REM --- Launch Client B (top-right, "Client P2") ---
start "" /D "%DIR_B%" "%EXE%" -t "Client P2" -w 960 -h 540 -x 960 -y 0

echo.
echo ============================================================
echo  Server + 2 clients launched.
echo  Client A  work dir: %DIR_A%
echo  Client B  work dir: %DIR_B%
echo  Event logs will be written to each work dir as events_*.jsonl
echo ============================================================
echo.
endlocal
