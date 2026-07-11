@echo off
REM ============================================================
REM RobotBuddy ESP-IDF Flash & Monitor Script
REM ============================================================
REM Usage:
REM   flash_monitor.bat          - Flash firmware and open serial monitor
REM   flash_monitor.bat flash     - Flash only (no monitor)
REM   flash_monitor.bat monitor   - Monitor only (no flash)
REM   flash_monitor.bat erase     - Erase flash
REM
REM First-time setup: Set COM_PORT to your ESP32-S3 serial port
REM   Common ports: COM3, COM4, COM5, etc.
REM   Check with: mode (in CMD) or Device Manager -> Ports

REM === Unset MSYSTEM (Git Bash sets this, ESP-IDF rejects it) ===
set MSYSTEM=

REM === ESP-IDF paths ===
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5
set IDF_TOOLS_PATH=C:\Users\Administrator\.espressif
set IDF_PYTHON_ENV_PATH=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.13_env
set ESP_ROM_ELF_DIR=C:\Users\Administrator\.espressif\tools\esp-rom-elfs\20241011

REM === Tool paths ===
set IDF_TOOLS=%IDF_TOOLS_PATH%\tools
set IDF_PYTHON=%IDF_TOOLS_PATH%\python_env\idf5.5_py3.13_env
set PATH=%IDF_TOOLS%\cmake\3.30.2\bin;%IDF_TOOLS%\ninja\1.12.1;%IDF_TOOLS%\idf-exe\1.0.3;%IDF_TOOLS%\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;%IDF_TOOLS%\xtensa-esp-elf-gdb\16.2_20250324\xtensa-esp-elf-gdb\bin;%IDF_TOOLS%\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;%IDF_TOOLS%\openocd-esp32\v0.12.0-esp32-20250422\openocd-esp32\bin;%IDF_TOOLS%\dfu-util\0.11\dfu-util;%IDF_PYTHON%\Scripts;%IDF_PYTHON%;%IDF_PATH%\tools;E:\Program Files\Git\cmd;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0

cd /d "F:\04 code\RobotBuddy\firmware"

REM === SERIAL PORT CONFIGURATION ===
REM Set your COM port here (check Device Manager -> Ports)
REM Default: COM3 (change to match your setup)
set COM_PORT=COM3
set BAUD_RATE=460800

echo ============================================================
echo  RobotBuddy Flash and Monitor
echo  Target: ESP32-S3-WROOM-1-N16R8
echo  Port:   %COM_PORT% at %BAUD_RATE% baud
echo ============================================================

if "%1"=="flash" goto FLASH_ONLY
if "%1"=="monitor" goto MONITOR_ONLY
if "%1"=="erase" goto ERASE_FLASH
goto FLASH_MONITOR

:FLASH_ONLY
echo.
echo === Flashing firmware to %COM_PORT% ===
%IDF_PYTHON%\Scripts\python.exe %IDF_PATH%\tools\idf.py -p %COM_PORT% -b %BAUD_RATE% flash
goto END

:MONITOR_ONLY
echo.
echo === Opening serial monitor on %COM_PORT% ===
%IDF_PYTHON%\Scripts\python.exe %IDF_PATH%\tools\idf.py -p %COM_PORT% monitor
goto END

:ERASE_FLASH
echo.
echo === Erasing flash on %COM_PORT% ===
%IDF_PYTHON%\Scripts\python.exe %IDF_PATH%\tools\idf.py -p %COM_PORT% erase-flash
goto END

:FLASH_MONITOR
echo.
echo === Flashing firmware and opening monitor ===
%IDF_PYTHON%\Scripts\python.exe %IDF_PATH%\tools\idf.py -p %COM_PORT% -b %BAUD_RATE% flash monitor
goto END

:END
echo.
echo Done.