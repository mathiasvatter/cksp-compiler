@echo off
setlocal

set "BUILD_DIR=cmake-build-release"
set "CC=C:/msys64/ucrt64/bin/gcc.exe"
set "CXX=C:/msys64/ucrt64/bin/g++.exe"
set "NINJA=C:/Program Files/JetBrains/CLion 2023.3.2/bin/ninja/win/x64/ninja.exe"
rem Build the project using Ninja
cmake -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=%CC%" "-DCMAKE_CXX_COMPILER=%CXX%" "-DCMAKE_MAKE_PROGRAM=%NINJA%" -G Ninja -S C:\Users\mathi\Documents\Scripting\ksp-compiler -B C:\Users\mathi\Documents\Scripting\ksp-compiler\%BUILD_DIR%
cmake --build "C:\Users\mathi\Documents\Scripting\ksp-compiler\%BUILD_DIR%" --target all -j 6

rem Check if cksp executable exists
if not exist "%BUILD_DIR%\cksp.exe" (
    echo Error: cksp executable not found in %BUILD_DIR%.
    exit /b 1
)

rem Run cksp --version and extract the version number
for /f "tokens=3" %%a in ('%BUILD_DIR%\cksp.exe --version') do set "VERSION=%%a"

rem Create a name for the release folder
set "RELEASES_DIR=_Releases"

if not exist "%RELEASES_DIR%" (
    mkdir "%RELEASES_DIR%"
)

set "RELEASE_DIR=%RELEASES_DIR%\cksp_v%VERSION%_release"
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
