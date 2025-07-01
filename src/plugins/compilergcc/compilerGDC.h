/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILER_GDC_H
#define COMPILER_GDC_H

#include "compiler.h"

class CompilerGDC : public Compiler
{
    public:
        CompilerGDC();
        ~CompilerGDC() override;
        AutoDetectResult AutoDetectInstallationDir() override;
    protected:
        Compiler* CreateCopy() override;
    private:
};

#endif // COMPILER_MINGW_H
