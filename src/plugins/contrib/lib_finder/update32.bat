@echo off
mkdir ..\..\..\devel32\share\CodeBlocks\lib_finder > nul 2>&1
mkdir ..\..\..\output32\share\CodeBlocks\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\devel32\share\CodeBlocks\lib_finder > nul 2>&1
copy /Y lib_finder\*.xml ..\..\..\output32\share\CodeBlocks\lib_finder > nul 2>&1
zip -j9 ..\..\..\devel32\share\CodeBlocks\lib_finder.zip manifest.xml
