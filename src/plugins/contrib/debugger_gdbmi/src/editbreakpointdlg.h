/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef EDITBREAKPOINT_H
#define EDITBREAKPOINT_H

#include "scrollingdialog.h"
//-#include "debugger_defs.h"
#include "definitions.h"

namespace dbg_mi
{
class EditBreakpointDlg : public wxScrollingDialog
{
    public:
        //EditBreakpointDlg(DebuggerBreakpoint &breakpoint, wxWindow* parent = 0);
        EditBreakpointDlg(Breakpoint& breakpoint, wxWindow* parent = 0);
        virtual ~EditBreakpointDlg();

        const Breakpoint& GetBreakpoint() const { return m_breakpoint; }
    protected:
        void OnUpdateUI(wxUpdateUIEvent& event);
        void EndModal(int retCode);

        Breakpoint& m_breakpoint;
    private:
        DECLARE_EVENT_TABLE()
};
} //namespace dbg_mi

#endif // EDITBREAKPOINT_H
