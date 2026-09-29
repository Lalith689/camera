@echo off
echo ===============================================================================
echo   SNYPTR-RAIL & ESP32-P4 TARGET RIG INTEGRATED SYSTEM LAUNCHER
echo ===============================================================================
echo   1. Dashboard URL     : http://localhost:8000/snyptr-rail/dashboard.html
echo   2. Live Video Feed   : http://localhost:8001/video_feed
echo   3. Metrics Telemetry : http://localhost:8001/metrics
echo   4. WebSocket Server  : ws://localhost:8765
echo ===============================================================================

echo Starting HTTP Dashboard Web Server on Port 8000...
start "Snyptr Rail Dashboard WebServer" cmd /c "python -m http.server 8000"

timeout /t 2 >nul
echo Opening Snyptr Rail Dashboard in browser...
start http://localhost:8000/snyptr-rail/dashboard.html

echo Starting Python CV & Target Hardware Backend (ESP32-P4 on COM15 / COM17, Servo on COM4)...
python usb_binary_test.py --port COM15 --servo-port COM4

pause
