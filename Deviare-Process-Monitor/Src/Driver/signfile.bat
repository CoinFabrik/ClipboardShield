@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
IF "%~1" == "" (
    ENDLOCAL
    ECHO Invalid target
    EXIT /B 1
)
"%WDKPATH%\bin\x86\SignTool.exe" sign /f "%~dp0\nektra_code_certificate.p12" /p dike2361 /fd SHA1 /ph /ac "%~dp0\VeriSign Class 3 Public Primary Certification Authority - G5.cer" /t http://timestamp.verisign.com/scripts/timstamp.dll "%~1"
IF NOT %ERRORLEVEL% == 0 (
    ENDLOCAL
    EXIT /B 1
)
ENDLOCAL
EXIT /B 0
