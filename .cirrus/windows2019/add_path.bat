@ECHO OFF

IF [%1]==[] GOTO usage
IF [%2]==[] GOTO usage

IF /i "x%1"=="xread" (
  IF [%3]==[] GOTO usage
  SET root=%2
  FOR /f "delims=" %%A IN (%3) DO (
    SET arg=%%A
    GOTO break
  )
) ELSE (
  FOR /F "useback tokens=*" %%A IN ('%2') DO SET arg=%%~A
  SET root=%1
)

:break

IF /i "x%root%"=="xuser" (
  SET key="HKEY_CURRENT_USER\Environment"
  GOTO user
)

IF /i "x%root%"=="xsystem" (
  SET key="HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
  GOTO system
)

GOTO usage
:user
:system

FOR /F "skip=2 tokens=1,2*" %%A IN ('REG QUERY %key% /V Path') DO (
  SET name=%%A
  SET value=%%C
)

IF "x%value:~-1%"=="x;" SET value=%value:~0,-1%

SET value="%value%;%arg%"

IF DEFINED NAME (
  REG ADD %key% /V Path /T REG_EXPAND_SZ /D %value% /F
)

GOTO:eof

:usage
ECHO Usage: %0 USER path
ECHO   or   %0 SYSTEM path
ECHO   or   %0 READ USER file-containing-path
ECHO   or   %0 READ SYSTEM file-containing-path
EXIT /B 1
