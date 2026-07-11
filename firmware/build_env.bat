@echo off
REM RobotBuddy Build Script
REM Fixes: unset MSYSTEM, set all ESP-IDF paths, then build

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

echo === Building RobotBuddy firmware ===
%IDF_PYTHON%\Scripts\python.exe %IDF_PATH%\tools\idf.py build
echo BUILD_EXIT=%ERRORLEVEL%

if exist "build\robotbuddy.bin" (
    echo.
    echo ============================================================
    echo  BUILD SUCCESS! Firmware binary created.
    echo ============================================================
    dir "build\robotbuddy.bin"
    dir "build\bootloader\bootloader.bin"
    dir "build\partition_table\partition-table.bin"
) else (
    echo.
    echo ============================================================
    echo  BUILD FAILED - checking error log
    echo ============================================================
    if exist "build\log\idf_py_stderr_output_*" (
        for %%f in (build\log\idf_py_stderr_output_*) do type "%%f"
    )
)