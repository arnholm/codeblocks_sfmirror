@echo off
rem
rem This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
rem http://www.gnu.org/licenses/gpl-3.0.html
rem
rem Copyright: 2008 Jens Lody
rem
rem $Revision: 7443 $
rem $Id: update.bat 7443 2011-09-01 16:29:16Z mortenmacfly $
rem $HeadURL: https://mortenmacfly@svn.berlios.de/svnroot/repos/codeblocks/trunk/src/plugins/contrib/IncrementalSearch/update.bat $
rem

set CB_DEVEL_DIR=devel%1
set CB_OUTPUT_DIR=output%1
set CB_DEVEL_RESDIR=%CB_DEVEL_DIR%\share\CodeBlocks
set CB_OUTPUT_RESDIR=%CB_OUTPUT_DIR%\share\CodeBlocks
md ..\..\..\%CB_DEVEL_RESDIR%\images\settings > nul 2>&1
md ..\..\..\%CB_OUTPUT_RESDIR%\images\settings > nul 2>&1
copy .\*.png ..\..\..\%CB_DEVEL_RESDIR%\images\settings\ > nul 2>&1
copy .\*.png ..\..\..\%CB_OUTPUT_RESDIR%\images\settings\ > nul 2>&1
exit 0

