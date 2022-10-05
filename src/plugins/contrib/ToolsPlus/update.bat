@echo off
set CB_DEVEL_DIR=devel%1
set CB_OUTPUT_DIR=output%1
set CB_DEVEL_RESDIR=%CB_DEVEL_DIR%\share\CodeBlocks
set CB_OUTPUT_RESDIR=%CB_OUTPUT_DIR%\share\CodeBlocks
md ..\..\..\%CB_DEVEL_RESDIR%\images\settings > nul 2>&1
md ..\..\..\%CB_OUTPUT_RESDIR%\images\settings > nul 2>&1
copy .\Resources\*.png ..\..\..\%CB_DEVEL_RESDIR%\images\settings\ > nul 2>&1
copy .\Resources\*.png ..\..\..\%CB_OUTPUT_RESDIR%\images\settings\ > nul 2>&1
exit 0
