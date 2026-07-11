@echo off
REM RobotBuddy ESP-IDF Environment Setup Script (CMD)
REM ===================================================
REM Usage: setup_idf_env.bat
REM        Then run: idf.py build
REM
REM This script configures the ESP-IDF v5.5 environment for RobotBuddy development.
REM It removes Git's MSYS/Mingw from PATH (ESP-IDF rejects it) and adds all
REM required ESP-IDF tools.

REM === Unset MSYSTEM (Git Bash sets this, ESP-IDF rejects it) ===
set MSYSTEM=

REM === ESP-IDF paths ===
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5
set IDF_TOOLS_PATH=C:\Users\Administrator\.espressif
set IDF_PYTHON_ENV_PATH=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.13_env
set ESP_ROM_ELF_DIR=C:\Users\Administrator\.espressif\tools\esp-rom-elfs\20241011

REM === Tool paths (prepend to system PATH) ===
set IDF_TOOLS=%IDF_TOOLS_PATH%\tools
set IDF_PYTHON=%IDF_TOOLS_PATH%\python_env\idf5.5_py3.13_env

set PATH=%IDF_TOOLS%\cmake\3.30.2\bin;%IDF_TOOLS%\ninja\1.12.1;%IDF_TOOLS%\idf-exe\1.0.3;%IDF_TOOLS%\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;%IDF_TOOLS%\xtensa-esp-elf-gdb\16.2_20250324\xtensa-esp-elf-gdb\bin;%IDF_TOOLS%\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;%IDF_TOOLS%\openocd-esp32\v0.12.0-esp32-20250422\openocd-esp32\bin;%IDF_TOOLS%\dfu-util\0.11\dfu-util;%IDF_PYTHON%\Scripts;%IDF_PYTHON%;%IDF_PATH%\tools;%PATH%

echo ============================================================
echo  RobotBuddy ESP-IDF Environment Ready
echo ============================================================
echo  IDF_PATH: %IDF_PATH%
echo  Target:   ESP32-S3 (N16R8)
echo  Python:   %IDF_PYTHON%\Scripts\python.exe
echo.
echo  Available commands:
echo    idf.py build         - Compile firmware
echo    idf.py flash          - Flash to device
echo    idf.py monitor        - Serial monitor
echo    idf.py flash monitor  - Flash and monitor
echo    idf.py menuconfig     - Configure options
echo ============================================================