@echo off
cls
md ..\..\..\devel32\share\CodeBlocks\images\wxsmith > nul 2>&1
md ..\..\..\output32\share\CodeBlocks\images\wxsmith > nul 2>&1
zip ..\..\..\devel32\share\CodeBlocks\wxsmith.zip manifest.xml
zip ..\..\..\output32\share\CodeBlocks\wxsmith.zip manifest.xml
copy wxwidgets\icons\*.png ..\..\..\devel32\share\CodeBlocks\images\wxsmith\ > nul 2>&1
copy wxwidgets\icons\*.png ..\..\..\output32\share\CodeBlocks\images\wxsmith\ > nul 2>&1
exit 0
