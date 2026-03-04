@echo off
setlocal

rem Dynamic path lookup for CI environments or local systems
@REM if defined CI (
    for /f "tokens=*" %%i in ('where cmake') do set "CMAKE=%%i"
    for /f "tokens=*" %%i in ('where ninja') do set "NINJA=%%i"
    set "CC=gcc"
    set "CXX=g++"
@REM ) else (
@REM     rem Local paths (customizable)
@REM     set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
@REM     set "NINJA=C:\Program Files\JetBrains\CLion 2024.3.2\bin\ninja\win\x64\ninja.exe"
@REM     set "CC=C:/msys64/ucrt64/bin/gcc.exe"
@REM     set "CXX=C:/msys64/ucrt64/bin/g++.exe"
@REM )

rem Check whether required tools are available
if not exist "%CMAKE%" (
    echo Error: cmake not found.
    exit /b 1
)
if not exist "%NINJA%" (
    echo Error: ninja not found.
    exit /b 1
)

rem Set build directory
set "BUILD_DIR=cmake-build-release"

rem Build project with CMake and Ninja
"%CMAKE%" -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER="%CC%" ^
    -DCMAKE_CXX_COMPILER="%CXX%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -G Ninja ^
    -S . ^
    -B "%BUILD_DIR%"
if errorlevel 1 (
    echo Error: CMake configuration failed.
    exit /b 1
)

"%CMAKE%" --build "%BUILD_DIR%" --target all -j 6
if errorlevel 1 (
    echo Error: Build failed.
    exit /b 1
)

rem Check whether the cksp binary exists
if not exist "%BUILD_DIR%\cksp.exe" (
    echo Error: cksp executable not found in %BUILD_DIR%.
    exit /b 1
)

rem Run cksp --version and extract the version number
for /f "tokens=3" %%a in ('%BUILD_DIR%\cksp.exe --version') do set "VERSION=%%a"
rem Determine the version directory dynamically
if "%VERSION%" == "" (
    echo Error: Version could not be determined.
    exit /b 1
)
echo Detected version: %VERSION%

rem Check whether this is a pre-release version
echo %VERSION% | find "-" >nul
if %ERRORLEVEL%==0 (
    set "VERSION_DIR=cksp_v%VERSION%"
) else (
    set "VERSION_DIR=cksp_v%VERSION%_release"
)
echo Using version directory: %VERSION_DIR%


rem Create a name for the release folder
set "RELEASES_DIR=_Releases"

if not exist "%RELEASES_DIR%" (
    mkdir "%RELEASES_DIR%"
)

set "RELEASE_DIR=%RELEASES_DIR%\%VERSION_DIR%"
set "ARM_DIR=macos_arm64"
set "INTEL_DIR=macos_x86_64"
set "WINDOWS_DIR=windows"

rem Create the release folder
if not exist "%RELEASE_DIR%" (
    mkdir "%RELEASE_DIR%"
)

rem Create a readme file
"%BUILD_DIR%\cksp.exe" --help > "%RELEASE_DIR%\readme.txt"

rem Create the architecture-specific directories
mkdir "%RELEASE_DIR%\%ARM_DIR%"
mkdir "%RELEASE_DIR%\%INTEL_DIR%"
mkdir "%RELEASE_DIR%\%WINDOWS_DIR%"

rem Copy the binary files to the release folder
copy "%BUILD_DIR%\cksp.exe" "%RELEASE_DIR%\%WINDOWS_DIR%\cksp.exe"

rem Zip it up (you need 7-Zip installed for this)
if exist "C:\Program Files\7-Zip\7z.exe" (
    "C:\Program Files\7-Zip\7z.exe" a -r -tzip "%RELEASE_DIR%.zip" "%RELEASE_DIR%" -xr!*.DS_Store
) else (
    echo Warning: 7-Zip not found. Please install it to create a zip archive.
)

echo Release files copied to %RELEASE_DIR%
endlocal
