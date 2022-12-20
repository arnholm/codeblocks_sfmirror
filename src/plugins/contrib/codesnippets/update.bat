@echo on
set CB_DEVEL_DIR=devel%1
set CB_OUTPUT_DIR=output%1
set CB_DEVEL_RESDIR=%CB_DEVEL_DIR%\share\CodeBlocks
set CB_OUTPUT_RESDIR=%CB_OUTPUT_DIR%\share\CodeBlocks
if exist ..\..\..\%CB_DEVEL_RESDIR%\plugins\codesnippets.zip del ..\..\..\%CB_DEVEL_RESDIR%\plugins\codesnippets.zip
if exist ..\..\..\%CB_OUTPUT_RESDIR%\plugins\codesnippets.zip del ..\..\..\%CB_OUTPUT_RESDIR%\plugins\codesnippets.zip
zip -j9 ..\..\..\%CB_DEVEL_RESDIR%\codesnippets.zip manifest.xml
if exist ..\..\..\%CB_OUTPUT_RESDIR% zip -j9 ..\..\..\%CB_OUTPUT_RESDIR%\codesnippets.zip manifest.xml
