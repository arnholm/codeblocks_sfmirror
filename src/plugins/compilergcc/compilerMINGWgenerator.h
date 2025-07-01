/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERMINGWGENERATOR_H
#define COMPILERMINGWGENERATOR_H

#include "compilercommandgenerator.h"

// Overriden to support PCH for GCC
class CompilerMINGWGenerator : public CompilerCommandGenerator
{
    public:
        CompilerMINGWGenerator();
        ~CompilerMINGWGenerator() override;
    protected:
        wxString SetupIncludeDirs(Compiler* compiler, ProjectBuildTarget* target) override;
    private:
        wxString m_VerStr;
};

#endif // COMPILERMINGWGENERATOR_H
