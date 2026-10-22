@echo off
rem Builds WinDjView x64 (static MFC, /O2 /GL /LTCG, AVX for Core i5-3230M).
rem Run from "x64 Native Tools Command Prompt for VS" (needs "C++ MFC for x64/x86").
setlocal

:: Always run relative to the script's directory
cd /d "%~dp0"

:: Ensure MSVC environment is loaded
where nmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] 'nmake' was not found in your PATH.
    echo Please run this script from the "x64 Native Tools Command Prompt for VS".
    exit /b 1
)

where msbuild >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] 'msbuild' was not found in your PATH.
    echo Please run this script from the "x64 Native Tools Command Prompt for VS".
    exit /b 1
)

set X64=1

echo =======================================================
echo [1/2] Building libdjvu (x64)...
echo =======================================================
pushd libdjvu
nmake /nologo X64=1 %*
set "LIB_ERR=%ERRORLEVEL%"
popd

:: Safely check exit code after popping directory
if %LIB_ERR% neq 0 (
    echo [ERROR] libdjvu compilation failed with error code %LIB_ERR%
    goto :fail
)

echo.
echo =======================================================
echo [2/2] Building WinDjView (x64) with MSBuild...
echo =======================================================
if /i "%~1"=="clean" (
    msbuild WinDjView.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Clean /nologo
) else (
    msbuild WinDjView.vcxproj /p:Configuration=Release /p:Platform=x64 -m /nologo
)
if %ERRORLEVEL% neq 0 (
    echo [ERROR] WinDjView compilation failed with error code %ERRORLEVEL%
    goto :fail
)

:: If a 'clean' command was passed, don't check for output exe
if /i "%~1"=="clean" (
    echo.
    echo Clean completed successfully.
    exit /b 0
)

:: Validate binary creation
if not exist "Release_x64\WinDjView.exe" (
    echo [ERROR] Build claimed success, but "Release_x64\WinDjView.exe" was not found!
    goto :fail
)

echo.
echo =======================================================
echo Build Succeeded: %~dp0Release_x64\WinDjView.exe
echo =======================================================
exit /b 0

:fail
echo.
echo =======================================================
echo BUILD FAILED
echo =======================================================
exit /b 1
