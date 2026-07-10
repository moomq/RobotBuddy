@echo off
REM ================================================================
REM RobotBuddy Development Environment Setup Script
REM ================================================================
REM This script checks and sets up the development environment
REM for building RobotBuddy firmware on Windows.
REM
REM Usage: setup.bat
REM ================================================================

echo ================================================================
echo   RobotBuddy Development Environment Setup
echo ================================================================
echo.

REM ---- Step 1: Check Python ----
echo [1/6] Checking Python...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Python not found! Please install Python 3.8+
    echo   Download: https://www.python.org/downloads/
    goto :error
) else (
    for /f "tokens=2 delims= " %%v in ('python --version 2^>^nul') do echo   [OK] Python %%v
)

REM ---- Step 2: Check Git ----
echo [2/6] Checking Git...
git --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Git not found! Please install Git 2.30+
    echo   Download: https://git-scm.com/download/win
    goto :error
) else (
    for /f "tokens=3" %%v in ('git --version') do echo   [OK] Git %%v
)

REM ---- Step 3: Check CMake ----
echo [3/6] Checking CMake...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [WARN] CMake not found globally. ESP-IDF includes its own CMake.
    echo   If you installed ESP-IDF, this is expected.
) else (
    for /f "tokens=3" %%v in ('cmake --version 2^>^nul') do echo   [OK] CMake %%v
)

REM ---- Step 4: Check ESP-IDF ----
echo [4/6] Checking ESP-IDF...
set "IDF_PATH=%USERPROFILE%\esp\esp-idf"
if exist "%IDF_PATH%\tools\idf.py" (
    echo   [OK] ESP-IDF found at: %IDF_PATH%
) else if defined IDF_PATH (
    echo   [OK] IDF_PATH is set to: %IDF_PATH%
) else (
    echo   [WARN] ESP-IDF not found at default path.
    echo   Please install ESP-IDF v5.4.1:
    echo   1. Download: https://dl.espressif.com/dl/esp-idf/
    echo   2. Or: git clone -b v5.4.1 --recursive https://github.com/espressif/esp-idf.git
    echo   3. Run: install.bat esp32s3
    echo   4. Run: export.bat
    echo.
    echo   After installation, set IDF_PATH environment variable
    echo   and re-run this script.
    goto :error
)

REM ---- Step 5: Check USB Drivers ----
echo [5/6] Checking USB Drivers...
set "CP210X_FOUND=0"
set "CH340_FOUND=0"
REM Note: Driver detection is complex on Windows, just show reminder
echo   [INFO] Make sure USB drivers are installed:
echo     - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
echo     - CH340:  http://www.wch-ic.com/downloads/CH341SER_EXE.html
echo     - ESP32-S3 uses built-in USB-JTAG (no driver needed on Windows 10+)

REM ---- Step 6: Python packages ----
echo [6/6] Checking Python packages...
python -c "import esptool" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [WARN] esptool not found. Install with: pip install esptool
) else (
    echo   [OK] esptool installed
)

python -c "import serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [WARN] pyserial not found. Install with: pip install pyserial
) else (
    echo   [OK] pyserial installed
)

echo.
echo ================================================================
echo   Environment Check Complete
echo ================================================================
echo.
echo Next steps:
echo   1. Open an ESP-IDF terminal (or run export.bat)
echo   2. cd to: %~dp0firmware
echo   3. Run: idf.py set-target esp32s3
echo   4. Run: idf.py build
echo.
echo If ESP-IDF is not installed, download it from:
echo   https://dl.espressif.com/dl/esp-idf/
echo ================================================================
goto :end

:error
echo.
echo [ERROR] Environment setup incomplete. Please fix the issues above.
pause
exit /b 1

:end
pause