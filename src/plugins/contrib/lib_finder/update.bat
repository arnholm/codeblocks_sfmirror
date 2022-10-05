@echo off
set CB_DEVEL_DIR=devel%1
set CB_OUTPUT_DIR=output%1
set CB_DEVEL_RESDIR=%CB_DEVEL_DIR%\share\CodeBlocks
set CB_OUTPUT_RESDIR=%CB_OUTPUT_DIR%\share\CodeBlocks
mkdir ..\..\..\%CB_DEVEL_RESDIR%\lib_finder > nul 2>&1
mkdir ..\..\..\%CB_OUTPUT_RESDIR%\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\%CB_DEVEL_RESDIR%\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\%CB_OUTPUT_RESDIR%\lib_finder > nul 2>&1
zip -j9 ..\..\..\%CB_DEVEL_RESDIR%\lib_finder.zip manifest.xml
