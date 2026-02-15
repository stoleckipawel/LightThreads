@echo off
REM Preview Deep Spark locally with Hugo (double-click this file)
cd /d "%~dp0"
echo Starting Hugo dev server in a new window...
start "Hugo" cmd /k "hugo server --bind 127.0.0.1 --port 1313 --baseURL http://localhost:1313/ --appendPort=false"
echo Waiting for server to start...
timeout /t 3 /nobreak >nul
echo Opening Chrome (or default browser)...
start "" chrome "http://localhost:1313/" 2>nul || start "" "http://localhost:1313/"
echo Hugo is running in the new window. Close the Hugo window to stop the server.
pause
