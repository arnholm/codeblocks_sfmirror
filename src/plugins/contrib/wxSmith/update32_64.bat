@echo off
cls
md ..\..\..\devel32_64\share\CodeBlocks\images\wxsmith > nul 2>&1
md ..\..\..\output32_64\share\CodeBlocks\images\wxsmith > nul 2>&1
zip ..\..\..\devel32_64\share\CodeBlocks\wxsmith.zip manifest.xml
zip ..\..\..\output32_64\share\CodeBlocks\wxsmith.zip manifest.xml
copy wxwidgets\icons\*.png ..\..\..\devel32_64\share\CodeBlocks\images\wxsmith\ > nul 2>&1
copy wxwidgets\icons\*.png ..\..\..\output32_64\share\CodeBlocks\images\wxsmith\ > nul 2>&1
exit 0
