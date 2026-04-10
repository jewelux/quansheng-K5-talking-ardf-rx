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

del /q /s *.o *.d firmware_uvk5_v1 firmware_uvk5_v1.bin firmware_uvk5_v1.packed.bin 2>nul
del /q firmware_uvk5_v1_*.bin firmware_uvk5_v1_*.packed.bin 2>nul
if defined PYTHON_CMD (
    make -B MY_PYTHON="%PYTHON_CMD%"
) else (
    make -B
)
if exist firmware_uvk5_v1.bin copy /Y firmware_uvk5_v1.bin firmware_uvk5_v1_MENU_TEST.bin >nul
if exist firmware_uvk5_v1.packed.bin copy /Y firmware_uvk5_v1.packed.bin firmware_uvk5_v1_MENU_TEST.packed.bin >nul
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set BUILDSTAMP=%%i
if exist firmware_uvk5_v1.bin copy /Y firmware_uvk5_v1.bin firmware_uvk5_v1_%BUILDSTAMP%.bin >nul
if exist firmware_uvk5_v1.packed.bin copy /Y firmware_uvk5_v1.packed.bin firmware_uvk5_v1_%BUILDSTAMP%.packed.bin >nul

pause
