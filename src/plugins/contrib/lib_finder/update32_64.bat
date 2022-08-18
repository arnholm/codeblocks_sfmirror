@echo off
mkdir ..\..\..\devel32_64\share\CodeBlocks\lib_finder > nul 2>&1
mkdir ..\..\..\output32_64\share\CodeBlocks\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\devel32_64\share\CodeBlocks\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\output32_64\share\CodeBlocks\lib_finder > nul 2>&1
zip -j9 ..\..\..\devel32_64\share\CodeBlocks\lib_finder.zip manifest.xml
