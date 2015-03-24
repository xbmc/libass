@ECHO OFF
ECHO Packaging archive for XBMC...
SETLOCAL ENABLEEXTENSIONS

SET VERSION=0.12.1
SET PACK_PATH=libass-%version%-win32
SET ZIP=..\deps\bin\7z\7za

IF EXIST %PACK_PATH% rmdir %PACK_PATH% /S /Q
mkdir "%PACK_PATH%\project\BuildDependencies\lib"
mkdir "%PACK_PATH%\project\BuildDependencies\include\ass"
mkdir "%PACK_PATH%\system\players\dvdplayer"

copy ..\..\libass\*.h "%PACK_PATH%\project\BuildDependencies\include\ass"
copy ..\VS2013\libs\Release\libass.dll "%PACK_PATH%\system\players\dvdplayer"
copy ..\VS2013\libs\Release\libass.lib "%PACK_PATH%\project\BuildDependencies\lib"
copy ..\..\README.md %PACK_PATH%\readme.txt

ECHO Create %PACK_PATH%.7z with '%PACK_PATH%'
%ZIP% a -t7z "%PACK_PATH%.7z" "%PACK_PATH%" 

PAUSE
