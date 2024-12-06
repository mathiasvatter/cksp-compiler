@echo off
setlocal

rem Setze das Build-Verzeichnis
set "BUILD_DIR=cmake-build-release"

rem Überprüfen, ob die cksp-Binärdatei existiert
if not exist "%BUILD_DIR%\cksp.exe" (
    echo Error: cksp executable not found in %BUILD_DIR%.
    exit /b 1
)

rem Run cksp --version and extract the version number
for /f "tokens=3" %%a in ('%BUILD_DIR%\cksp.exe --version') do set "VERSION=%%a"

rem Dynamisch die Version Directory ermitteln
if "%VERSION%" == "" (
    echo Error: Version could not be determined.
    exit /b 1
)
echo Detected version: %VERSION%

rem Prüfen, ob es sich um eine Pre-Release-Version handelt
echo %VERSION% | find "-" >nul
if %ERRORLEVEL%==0 (
    set "VERSION_DIR=cksp_v%VERSION%"
) else (
    set "VERSION_DIR=cksp_v%VERSION%_release"
)

rem Ausgabe der Version Directory
echo %VERSION_DIR%
endlocal
