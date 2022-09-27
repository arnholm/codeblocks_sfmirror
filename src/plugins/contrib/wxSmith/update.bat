@echo off
cls
set CB_DEVEL_DIR=devel%1
set CB_OUTPUT_DIR=output%1
set CB_DEVEL_RESDIR=%CB_DEVEL_DIR%\share\CodeBlocks
set CB_OUTPUT_RESDIR=%CB_OUTPUT_DIR%\share\CodeBlocks
md  ..\..\..\%CB_DEVEL_RESDIR%\images\wxsmith > nul 2>&1
md  ..\..\..\%CB_OUTPUT_RESDIR%\images\wxsmith > nul 2>&1
zip ..\..\..\%CB_DEVEL_RESDIR%\wxsmith.zip manifest.xml
zip ..\..\..\%CB_OUTPUT_RESDIR%\wxsmith.zip manifest.xml
copy wxwidgets\icons\*.png ..\..\..\%CB_DEVEL_RESDIR%\images\wxsmith\ > nul 2>&1
copy wxwidgets\icons\*.png ..\..\..\%CB_OUTPUT_RESDIR%\images\wxsmith\ > nul 2>&1
copy wxwidgets\icons\*.svg ..\..\..\%CB_DEVEL_RESDIR%\images\wxsmith\ > nul 2>&1
copy wxwidgets\icons\*.svg ..\..\..\%CB_OUTPUT_RESDIR%\images\wxsmith\ > nul 2>&1
exit 0
