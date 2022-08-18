@echo off
del ..\..\..\output32\share\CodeBlocks\plugins\codesnippets.exe > nul 2>&1
zip -j9 ..\..\..\devel32\share\CodeBlocks\codesnippets.zip manifest.xml > nul 2>&1
md ..\..\..\devel32\share\CodeBlocks\images\codesnippets > nul 2>&1
copy .\resources\*.png ..\..\..\devel32\share\CodeBlocks\images\codesnippets\ > nul 2>&1
md ..\..\..\output32\share\CodeBlocks\images\codesnippets > nul 2>&1
copy .\resources\*.png ..\..\..\output32\share\CodeBlocks\images\codesnippets\ > nul 2>&1
