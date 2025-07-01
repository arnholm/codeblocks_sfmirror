/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERMSVC17_H_
#define COMPILERMSVC17_H_

#include "compiler.h"

class CompilerMSVC17 : public Compiler
{
public:
    CompilerMSVC17();
    ~CompilerMSVC17() override;
    AutoDetectResult AutoDetectInstallationDir() override;

protected:
    Compiler* CreateCopy() override;
};

#endif // COMPILERMSVC17_H_
