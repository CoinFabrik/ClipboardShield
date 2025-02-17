@ECHO OFF
SETLOCAL enableextensions

REM -----------------------------------------------------------------------
REM Setup variables...
IF /I "%1" == "build" (
    SET __buildAction=1
) ELSE IF /I "%1" == "rebuild" (
    SET __buildAction=2
) ELSE IF /I "%1" == "clean" (
    SET __buildAction=3
) ELSE (
    ECHO Error: Invalid action
    ENDLOCAL
    EXIT /b 1
)

IF /I "%2" == "Win32" (
    SET __buildPlatform=-WIN7
    SET __buildFolder=x86
    SET __buildPlatformInt=i386
    SET __buildPlatformWdk=x86
) ELSE IF /I "%2" == "x64" (
    SET __buildPlatform=-WIN7A64
    SET __buildFolder=x64
    SET __buildPlatformInt=amd64
    SET __buildPlatformWdk=amd64
) ELSE (
    ENDLOCAL
    ECHO Error: Invalid platform
    EXIT /b 1
)

IF /I "%3" == "debug" (
    SET __buildConfig=chk
) ELSE IF /I "%3" == "release" (
    SET __buildConfig=fre
) ELSE (
    ENDLOCAL
    ECHO Error: Invalid configuration
    EXIT /b 1
)

REM -----------------------------------------------------------------------
REM CleanUp
SET _targetFolder=bin
SET _targetName=DeviarePD


IF "%__buildAction%" NEQ "1" (
    RD /S /Q  "%~dp0\%_targetFolder%\%__buildFolder%"              >NUL 2>NUL
    RD /S /Q  "%~dp0\obj%__buildConfig%_win7_%__buildPlatformWdk%" >NUL 2>NUL
)
DEL /F /Q "%~dp0\build%__buildConfig%_win7_%__buildPlatformWdk%.log"           >NUL 2>NUL
DEL /F /Q "%~dp0\PREfast_defects_%__buildConfig%_win7_%__buildPlatformWdk%*.*" >NUL 2>NUL
IF "%__buildAction%" == "3" (
    ENDLOCAL
    ECHO Cleanup completed
    EXIT /b 0
)

REM -----------------------------------------------------------------------
REM Setup environment


::IF "%WDKPATH%" == "" (
::    ENDLOCAL
::    ECHO Error: WDK not found or not configured
::    EXIT /b 1
::)
SET W7BASE=C:\WinDDK\7600.16385.1

REM -----------------------------------------------------------------------
REM Create target folders
MD "%~dp0\%_targetFolder%"                 >NUL 2>NUL
MD "%~dp0\%_targetFolder%\%__buildFolder%" >NUL 2>NUL

REM -----------------------------------------------------------------------
SET __targetFileTimeStamp=X
SET __targetFile=%~dp0\obj%__buildConfig%_win7_%__buildPlatformWdk%\%__buildPlatformInt%\%_targetName%.sys
IF EXIST "%~dp0\%_targetFolder%\%__buildFolder%\%_targetName%.sys" (
    FOR %%X in ("%__targetFile%") DO SET __targetFileTimeStamp=%%~tX
)

REM -----------------------------------------------------------------------
REM Build project
SET __buildCmdLine=
IF "%__buildAction%" == "2" (
    SET __buildCmdLine=-cZ
)
CALL "%~dp0\ddkbuild.cmd" %__buildPlatform% %__buildConfig% . %__buildCmdLine% -PREFAST
IF ERRORLEVEL 1 (
    ECHO Error: Compile failed
    ENDLOCAL
    EXIT /b 1
)
IF "%__buildAction%" == "3" (
    SET __targetFileTimeStamp2=Y
) ELSE (
    FOR %%X in ("%__targetFile%") DO SET __targetFileTimeStamp2=%%~tX
)

IF "%__targetFileTimeStamp%" NEQ "%__targetFileTimeStamp2%" (
    REM -----------------------------------------------------------------------
    REM Copy files...
    COPY /Y "%__targetFile%" "%~dp0\%_targetFolder%\%__buildFolder%" >NUL 2>NUL

    REM -----------------------------------------------------------------------
    REM Digitally sign the driver
    IF EXIST "%~dp0\NektraDigitalCertificates\signfile.bat" (
        CALL "%~dp0\NektraDigitalCertificates\signfile.bat" "%~dp0\%_targetFolder%\%__buildFolder%\%_targetName%.sys"
        IF NOT %ERRORLEVEL% == 0 (
            ECHO Error: Signing failed
            ENDLOCAL
            EXIT /b 1
        )
    )
)

REM -----------------------------------------------------------------------
REM Done
ENDLOCAL
EXIT /b 0
