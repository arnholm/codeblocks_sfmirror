@echo off
md ..\..\..\devel32\share\CodeBlocks\images\settings > nul 2>&1
md ..\..\..\output32\share\CodeBlocks\images\settings > nul 2>&1
copy .\Resources\*.png ..\..\..\devel32\share\CodeBlocks\images\settings\ > nul 2>&1
copy .\Resources\*.png ..\..\..\output32\share\CodeBlocks\images\settings\ > nul 2>&1
exit 0
