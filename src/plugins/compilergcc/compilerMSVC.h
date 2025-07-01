/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERMSVC_H
#define COMPILERMSVC_H

#include "compiler.h"

class CompilerMSVC : public Compiler
{
	public:
		CompilerMSVC();
		~CompilerMSVC() override;
        AutoDetectResult AutoDetectInstallationDir() override;
	protected:
        Compiler * CreateCopy() override;
	private:
};

#endif // COMPILERMSVC_H
