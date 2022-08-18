@echo off
md ..\..\..\devel32_64\share\CodeBlocks\images\settings > nul 2>&1
md ..\..\..\output32_64\share\CodeBlocks\images\settings > nul 2>&1
copy .\Resources\*.png ..\..\..\devel32_64\share\CodeBlocks\images\settings\ > nul 2>&1
copy .\Resources\*.png ..\..\..\output32_64\share\CodeBlocks\images\settings\ > nul 2>&1
exit 0
