@echo off
rem Builds WinDjView x64 (static MFC, /O2 /GL /LTCG, AVX for Core i5-3230M).
rem Run from "x64 Native Tools Command Prompt for VS" (needs the "C++ MFC for x64/x86" component).
setlocal
set X64=1
pushd libdjvu
nmake /nologo X64=1 || goto :fail
popd
nmake /nologo X64=1 || goto :fail
echo.
echo Done: Release_x64\WinDjView.exe
exit /b 0
:fail
echo BUILD FAILED
exit /b 1
