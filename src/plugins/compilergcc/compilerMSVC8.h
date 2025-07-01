/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERMSVC8_H
#define COMPILERMSVC8_H

#include "compiler.h"

class CompilerMSVC8 : public Compiler
{
    public:
        CompilerMSVC8();
        ~CompilerMSVC8() override;
        AutoDetectResult AutoDetectInstallationDir() override;
    protected:
        Compiler * CreateCopy() override;
    private:
};

#endif // COMPILERMSVC8_H
