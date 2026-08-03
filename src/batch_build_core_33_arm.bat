@echo off
rem ----------------------------------------
rem Setup C::B root folder of *binaries* (!)
rem ----------------------------------------
call DevelFolder.bat WinLibs 16.1.0

rem -------------------------------------------
rem Usually below here no changes are required.
rem -------------------------------------------
if not exist "%CB_64%" goto ErrNoCB
set OLD_PATH=%PATH%

set BUILD_TYPE=--build
if "%1"=="r"        set BUILD_TYPE=--rebuild
if "%1"=="-r"       set BUILD_TYPE=--rebuild
if "%1"=="rebuild"  set BUILD_TYPE=--rebuild
if "%1"=="-rebuild" set BUILD_TYPE=--rebuild
if "%1"=="c"        set BUILD_TYPE=--clean
if "%1"=="-c"       set BUILD_TYPE=--clean
if "%1"=="clean"    set BUILD_TYPE=--clean
if "%1"=="-clean"   set BUILD_TYPE=--clean
set PRIO=
if "%1"=="p"        set PRIO=/BELOWNORMAL
if "%1"=="-p"       set PRIO=/BELOWNORMAL
if "%2"=="p"        set PRIO=/BELOWNORMAL
if "%2"=="-p"       set PRIO=/BELOWNORMAL

set CB_EXE="%CB_64%\codeblocks.exe"
set CB_PARAMS=--batch-build-notify --no-batch-window-close
set CB_CMD=%BUILD_TYPE% "%~dp0CodeBlocks_wx33_arm.cbp"

:64Bit
set PATH=%CB_64%;%MINGW_LLVMARM%;%MINGW_LLVMARM%\bin;%OLD_PATH%
set START_CMD=start "Code::Blocks Core ARM build (wx3.3.x)" /D"%~dp0" %PRIO% /MIN /B
set CB_TARGET=--target=All
if not exist "%MINGW_LLVMARM%" goto ErrNoGCC
%START_CMD% %CB_EXE% %CB_PARAMS% %CB_TARGET% %CB_CMD%
echo Do not forget to run "update33_arm.bat" after successful build!
goto TheEnd

:ErrNoCB
echo Error: C::B root folder '%CB_64%' not found. Adjust batch file accordingly
goto TheEnd

:ErrNoGCC
echo Error: GCC root folder '%MINGW_LLVMARM%' not found. Adjust batch file accordingly
goto TheEnd

:TheEnd
