@ECHO OFF
SETLOCAL
CSCRIPT autoincrement.vbs
IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL%

IF /I "%1" == "debug" (
	CALL make.bat rebuild Win32 debug
	IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL%
	CALL make.bat rebuild x64 debug
	IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL%
) ELSE (
	CALL make.bat rebuild Win32 release
	IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL%
	CALL make.bat rebuild x64 release
	IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL%
)
