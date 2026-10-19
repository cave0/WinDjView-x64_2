@echo off
setlocal
set MAKETARGET=all
if "%1"=="r" set MAKETARGET=rebuild
if "%2"=="r" set MAKETARGET=rebuild
if "%1"=="n" (
	echo Building with CL7
	nmake /nologo %MAKETARGET% "NEWCL=1"
	if errorlevel 1 goto end
	nmake /nologo %MAKETARGET% "UNICODE=1" "NEWCL=1"
	if errorlevel 1 goto end
) else (
	echo Building with CL6
	call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
	nmake /nologo %MAKETARGET%
	if errorlevel 1 goto end
	nmake /nologo %MAKETARGET% "DEBUG=1"
	if errorlevel 1 goto end
	nmake /nologo %MAKETARGET% "UNICODE=1"
	if errorlevel 1 goto end
	nmake /nologo %MAKETARGET% "UNICODE=1" "DEBUG=1"
	if errorlevel 1 goto end
)
:end
endlocal
pause