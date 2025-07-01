/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILER_MINGW_H
#define COMPILER_MINGW_H

#include <wx/intl.h>
#include "compiler.h"

class CompilerMINGW : public Compiler
{
    public:
        // added arguments to ctor so we can derive other gcc-flavours directly
        // from MinGW (e.g. the cygwin compiler is derived from this one).
        CompilerMINGW(const wxString& name = _("GNU GCC Compiler"), const wxString& ID = _T("gcc"));
        ~CompilerMINGW() override;
        AutoDetectResult AutoDetectInstallationDir() override;
        CompilerCommandGenerator* GetCommandGenerator(cbProject *project) override;
    protected:
        Compiler* CreateCopy() override;
        void SetVersionString() override;
    private:
};

#endif // COMPILER_MINGW_H
