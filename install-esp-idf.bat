@echo off
REM ================================================================
REM RobotBuddy ESP-IDF Installation Script
REM ================================================================
REM This script installs ESP-IDF v5.4.1 and sets up the
REM development environment for building RobotBuddy firmware.
REM
REM Prerequisites:
REM   - Python 3.8+ (already installed)
REM   - Git 2.30+ (already installed)
REM   - Windows 10/11
REM
REM Usage: install-esp-idf.bat
REM ================================================================

echo ================================================================
echo   RobotBuddy ESP-IDF v5.4.1 Installation
echo ================================================================
echo.

REM ---- Configuration ----
set "IDF_VERSION=v5.4.1"
set "IDF_DIR=C:\Espressif\frameworks\esp-idf-%IDF_VERSION%"
set "IDF_TOOLS_DIR=C:\Espressif\tools"

echo Target: ESP-IDF %IDF_VERSION%
echo Install path: %IDF_DIR%
echo Tools path: %IDF_TOOLS_DIR%
echo.

REM ---- Step 1: Check prerequisites ----
echo [1/5] Checking prerequisites...

git --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Git not found! Please install Git first.
    echo   Download: https://git-scm.com/download/win
    pause
    exit /b 1
)
for /f "tokens=3" %%v in ('git --version') do echo   [OK] Git %%v

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Python not found! Please install Python 3.8+.
    echo   Download: https://www.python.org/downloads/
    pause
    exit /b 1
)
for /f "tokens=2" %%v in ('python --version 2^>nul') do echo   [OK] Python %%v

REM ---- Step 2: Clone ESP-IDF ----
echo.
echo [2/5] Cloning ESP-IDF %IDF_VERSION%...
echo This will take 5-15 minutes depending on your internet speed.
echo.

if exist "%IDF_DIR%" (
    echo   ESP-IDF directory already exists: %IDF_DIR%
    echo   Checking if it's a valid repository...
    if exist "%IDF_DIR%\tools\idf.py" (
        echo   [OK] ESP-IDF installation found!
        goto :install_tools
    ) else (
        echo   [WARN] Directory exists but incomplete. Will re-clone.
        rmdir /s /q "%IDF_DIR%"
    )
)

mkdir "%IDF_DIR%\.." 2>nul
echo   Cloning repository (with submodules, this takes time)...
git clone -b %IDF_VERSION% --recursive --depth 1 https://github.com/espressif/esp-idf.git "%IDF_DIR%"
if %errorlevel% neq 0 (
    echo   [ERROR] Git clone failed!
    echo   Try running manually:
    echo   git clone -b %IDF_VERSION% --recursive https://github.com/espressif/esp-idf.git "%IDF_DIR%"
    pause
    exit /b 1
)
echo   [OK] ESP-IDF cloned successfully!

:install_tools
REM ---- Step 3: Install ESP-IDF tools ----
echo.
echo [3/5] Installing ESP-IDF tools (this takes 5-10 minutes)...
echo.

cd /d "%IDF_DIR%"
call install.bat esp32s3
if %errorlevel% neq 0 (
    echo   [ERROR] Tool installation failed!
    pause
    exit /b 1
)
echo   [OK] ESP-IDF tools installed!

REM ---- Step 4: Set environment variables ----
echo.
echo [4/5] Setting environment variables...

REM Set IDF_PATH for current session
setx IDF_PATH "%IDF_DIR%" >nul
echo   [OK] IDF_PATH set to: %IDF_DIR%

REM Add to PATH if not already there
echo   Adding ESP-IDF to system PATH...
setx PATH "%PATH%;%IDF_DIR%\tools;%IDF_TOOLS_DIR%\xtensa-esp-elf-clang\esp-15.2.0_20240904\xtensa-esp-elf-clang\bin;%IDF_TOOLS_DIR%\xtensa-esp32s3-elf\esp-13.2.0_20240530\xtensa-esp32s3-elf\bin;%IDF_TOOLS_DIR%\cmake\3.24.0\bin;%IDF_TOOLS_DIR%\ninja\1.12.1;%IDF_TOOLS_DIR%\openocd-esp32\v0.12.0-esp32-20240821\openocd-esp32\bin" >nul 2>&1

echo.
echo   [IMPORTANT] Please close and reopen your terminal for PATH changes to take effect!

REM ---- Step 5: Verify installation ----
echo.
echo [5/5] Verifying installation...

cd /d "%IDF_DIR%"
call export.bat >nul 2>&1

idf.py --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [WARN] Could not verify idf.py. You may need to run export.bat manually.
) else (
    for /f "tokens=2" %%v in ('idf.py --version 2^>nul') do echo   [OK] ESP-IDF %%v
)

echo.
echo ================================================================
echo   Installation Complete!
echo ================================================================
echo.
echo Next steps:
echo   1. Close and reopen your terminal
echo   2. Run: C:\Espressif\frameworks\esp-idf-v5.4.1\export.bat
echo   3. cd to: %~dp0firmware
echo   4. Run: idf.py set-target esp32s3
echo   5. Run: idf.py build
echo.
echo To set up ESP-IDF in any new terminal, run:
echo   C:\Espressif\frameworks\esp-idf-v5.4.1\export.bat
echo.
pause