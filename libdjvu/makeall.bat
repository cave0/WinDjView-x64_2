@echo off
setlocal
set MAKETARGET=all
if "%1"=="r" set MAKETARGET=rebuild
if "%2"=="r" set MAKETARGET=rebuild
:: Check if MSVC is already initialized in current shell
where cl.exe >nul 2>nul
if %ERRORLEVEL% neq 0 (
	:: Locate VS2022/VS2019 via vswhere if available
	set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
	if exist "%VSWHERE%" (
		for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
			if exist "%%i\VC\Auxiliary\Build\vcvars32.bat" (
				call "%%i\VC\Auxiliary\Build\vcvars32.bat"
			)
		)
	)
)

echo Building with modern MSVC (NEWCL)
nmake /nologo %MAKETARGET% "NEWCL=1"
if errorlevel 1 goto end
nmake /nologo %MAKETARGET% "DEBUG=1" "NEWCL=1"
if errorlevel 1 goto end
nmake /nologo %MAKETARGET% "UNICODE=1" "NEWCL=1"
if errorlevel 1 goto end
nmake /nologo %MAKETARGET% "UNICODE=1" "DEBUG=1" "NEWCL=1"
if errorlevel 1 goto end
:end
endlocal
pause
