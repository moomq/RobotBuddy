# RobotBuddy ESP-IDF Environment Setup Script (PowerShell)
# ============================================================
# Usage: . .\setup_idf_env.ps1
#        (dot-source to import into current session)
#
# This script configures the ESP-IDF v5.5 environment for RobotBuddy development.
# It removes Git's MSYS/Mingw from PATH (ESP-IDF rejects it) and adds all
# required ESP-IDF tools.

# === ESP-IDF paths ===
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5"
$env:IDF_TOOLS_PATH = "C:\Users\Administrator\.espressif"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\Administrator\.espressif\python_env\idf5.5_py3.13_env"
$env:ESP_ROM_ELF_DIR = "C:\Users\Administrator\.espressif\tools\esp-rom-elfs\20241011"

# === CRITICAL: Unset MSYSTEM (Git Bash sets this, ESP-IDF rejects it) ===
$env:MSYSTEM = $null

# === Tool bin directories ===
$toolPaths = @(
    "$env:IDF_TOOLS_PATH\tools\cmake\3.30.2\bin",
    "$env:IDF_TOOLS_PATH\tools\ninja\1.12.1",
    "$env:IDF_TOOLS_PATH\tools\idf-exe\1.0.3",
    "$env:IDF_TOOLS_PATH\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin",
    "$env:IDF_TOOLS_PATH\tools\xtensa-esp-elf-gdb\16.2_20250324\xtensa-esp-elf-gdb\bin",
    "$env:IDF_TOOLS_PATH\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin",
    "$env:IDF_TOOLS_PATH\tools\openocd-esp32\v0.12.0-esp32-20250422\openocd-esp32\bin",
    "$env:IDF_TOOLS_PATH\tools\dfu-util\0.11\dfu-util",
    "$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env\Scripts",
    "$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env",
    "$env:IDF_PATH\tools"
)

# === Clean PATH: remove Git's mingw/usr paths that trigger MSYS detection ===
$cleanPath = ($env:PATH -split ";" | Where-Object {
    $_ -notmatch "mingw64\\bin" -and
    $_ -notmatch "Git\\usr\\bin" -and
    $_ -notmatch "Git\\usr\\local\\bin"
})

# === Add Git cmd (but not mingw/usr) back ===
$gitCmd = "E:\Program Files\Git\cmd"

# === Build final PATH ===
$env:PATH = ($toolPaths + @($gitCmd) + $cleanPath) -join ";"

# === Verify tools ===
Write-Host "=== RobotBuddy ESP-IDF Environment ===" -ForegroundColor Cyan
Write-Host "IDF_PATH:     $env:IDF_PATH" -ForegroundColor Green
Write-Host "Target:       ESP32-S3-WROOM-1-N16R8" -ForegroundColor Green
Write-Host "Firmware dir: F:\04 code\RobotBuddy\firmware" -ForegroundColor Green

$tools = @("idf.py", "cmake", "ninja", "xtensa-esp32s3-elf-gcc", "python", "git")

$allOk = $true
foreach ($tool in $tools) {
    $found = Get-Command $tool -ErrorAction SilentlyContinue
    if ($found) {
        Write-Host "  [OK] $tool -> $($found.Source)" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $tool" -ForegroundColor Red
        $allOk = $false
    }
}

if ($allOk) {
    Write-Host "`nEnvironment ready! Available commands:" -ForegroundColor Green
    Write-Host "  idf.py build          - Compile firmware" -ForegroundColor Yellow
    Write-Host "  idf.py flash          - Flash to device" -ForegroundColor Yellow
    Write-Host "  idf.py monitor        - Serial monitor" -ForegroundColor Yellow
    Write-Host "  idf.py flash monitor  - Flash and monitor" -ForegroundColor Yellow
    Write-Host "  idf.py menuconfig     - Configure options" -ForegroundColor Yellow
    Write-Host "  idf.py set-target esp32s3 - Set target chip" -ForegroundColor Yellow
} else {
    Write-Host "`nSome tools are missing. Check the paths above." -ForegroundColor Red
}