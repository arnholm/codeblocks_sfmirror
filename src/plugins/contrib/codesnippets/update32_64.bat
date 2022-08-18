@echo off
del ..\..\..\output32_64\share\CodeBlocks\plugins\codesnippets.exe > nul 2>&1
zip -j9 ..\..\..\devel32_64\share\CodeBlocks\codesnippets.zip manifest.xml > nul 2>&1
md ..\..\..\devel32_64\share\CodeBlocks\images\codesnippets > nul 2>&1
copy .\resources\*.png ..\..\..\devel32_64\share\CodeBlocks\images\codesnippets\ > nul 2>&1
md ..\..\..\output32_64\share\CodeBlocks\images\codesnippets > nul 2>&1
copy .\resources\*.png ..\..\..\output32_64\share\CodeBlocks\images\codesnippets\ > nul 2>&1
