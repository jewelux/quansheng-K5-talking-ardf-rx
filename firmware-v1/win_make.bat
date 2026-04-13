@echo off
setlocal

set "PYTHON_PATHS=C:\Windows;C:\Users\User\AppData\Local\Microsoft\WindowsApps"
set "PATH=C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin;C:\Program Files (x86)\GnuWin32\bin;C:\Program Files\Git\usr\bin;%PYTHON_PATHS%;%PATH%"
set "BUILDROOT=%~sdp0"
cd /d "%BUILDROOT%"

set "PYTHON_CMD="
if exist "C:\Windows\py.exe" set "PYTHON_CMD=C:\Windows\py.exe"
if not defined PYTHON_CMD if exist "C:\Users\User\AppData\Local\Microsoft\WindowsApps\python.exe" set "PYTHON_CMD=C:\Users\User\AppData\Local\Microsoft\WindowsApps\python.exe"
if not defined PYTHON_CMD if exist "C:\Users\User\AppData\Local\Microsoft\WindowsApps\python3.exe" set "PYTHON_CMD=C:\Users\User\AppData\Local\Microsoft\WindowsApps\python3.exe"

REM Ausgabeverzeichnis (neben firmware-source\)
set "OUTDIR=%BUILDROOT%..\build-output"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM Alte Build-Artefakte loeschen (via Makefile, sauber und ohne /s Rauschen)
make clean 2>nul
del /q firmware_uvk5_v1 firmware_uvk5_v1.bin firmware_uvk5_v1.packed.bin 2>nul

if defined PYTHON_CMD (
    make -B MY_PYTHON="%PYTHON_CMD%"
) else (
    make -B
)

REM Firmware-Binaries ins Ausgabeverzeichnis verschieben
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set BUILDSTAMP=%%i
if exist firmware_uvk5_v1.bin (
    move /Y firmware_uvk5_v1.bin "%OUTDIR%\firmware_uvk5_v1.bin" >nul
    copy /Y "%OUTDIR%\firmware_uvk5_v1.bin" "%OUTDIR%\firmware_uvk5_v1_%BUILDSTAMP%.bin" >nul
)
if exist firmware_uvk5_v1.packed.bin (
    move /Y firmware_uvk5_v1.packed.bin "%OUTDIR%\firmware_uvk5_v1.packed.bin" >nul
    copy /Y "%OUTDIR%\firmware_uvk5_v1.packed.bin" "%OUTDIR%\firmware_uvk5_v1_%BUILDSTAMP%.packed.bin" >nul
)

REM Quellverzeichnis aufraeumen (Objekt-Dateien, ELF, Dependencies)
make clean 2>nul
del /q firmware_uvk5_v1 2>nul

echo.
echo   Alle Firmware-Dateien liegen in:
echo     %OUTDIR%
echo.
echo   Zum Flashen diese Datei verwenden:
echo     %OUTDIR%\firmware_uvk5_v1_%BUILDSTAMP%.packed.bin
echo.

pause
