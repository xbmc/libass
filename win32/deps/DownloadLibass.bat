@ECHO OFF

SETLOCAL

SET VERSION=0.12.1
SET LIBASS=%VERSION%.zip

SET CUR_PATH=%CD%
SET TMP_PATH=%CD%\scripts\tmp
SET SRC_PATH=%CD%\..\..\libass

rem can't run rmdir and md back to back. access denied error otherwise.
IF EXIST %TMP_PATH% rmdir %TMP_PATH% /S /Q
IF EXIST %SRC_PATH% rmdir %SRC_PATH% /S /Q

SET WGET=%CUR_PATH%\bin\wget
SET ZIP=%CUR_PATH%\bin\7z\7za
SET DL_PATH="%CD%\downloads"

IF NOT EXIST %DL_PATH% md %DL_PATH%

md %TMP_PATH%

CD %DL_PATH% || EXIT /B 10
SET RetryDownload=YES
:startDownloadingFile
IF EXIST %LIBASS% (
  ECHO Using downloaded libass %LIBASS%
) ELSE (
  ECHO Downloading libass %LIBASS%...
  %WGET% "https://github.com/libass/libass/archive/%LIBASS%" "--no-check-certificate" "--output-document=%LIBASS%"|| EXIT /B 7
  TITLE Getting libass %LIBASS%
)

copy /b "%LIBASS%" "%TMP_PATH%" || EXIT /B 5
PUSHD "%TMP_PATH%" || EXIT /B 10
%ZIP% x %LIBASS% || (
  IF %RetryDownload%==YES (
    POPD || EXIT /B 5
    ECHO WARNNING! Can't extract files from archive %LIBASS%!
    ECHO WARNNING! Deleting %LIBASS% and will retry downloading.
    del /f "%LIBASS%"
    SET RetryDownload=NO
    GOTO startDownloadingFile
  )
  exit /B 6
)

XCOPY "libass-%VERSION%\libass\*" "%SRC_PATH%\" /E /I /Y /F /R /H /K || EXIT /B 5

ECHO.
ECHO Done %LIBASS%.
POPD || EXIT /B 10

CD %CUR_PATH%

rmdir %TMP_PATH% /S /Q
EXIT /B 0
