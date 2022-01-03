/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef EDITKEYWORDSDLG_H
#define EDITKEYWORDSDLG_H

#include "scrollingdialog.h"
#include "globals.h"

class wxSpinCtrl;
class wxTextCtrl;
class EditorColourSet;

class EditKeywordsDlg : public wxScrollingDialog
{
    public:
        EditKeywordsDlg(wxWindow* parent, EditorColourSet* theme, HighlightLanguage lang, const wxArrayString& descr);
        ~EditKeywordsDlg();

    protected:
        void OnExit(wxCommandEvent& event);
        void OnSetChange(wxSpinEvent& event);

        wxSpinCtrl* spnSet;
        wxTextCtrl* txtKeywords;

    private:
        void SaveKeywords(int index);
        void UpdateDlg();

        EditorColourSet* m_pTheme;
        HighlightLanguage m_Lang;
        int m_LastSet;
        const wxArrayString& descriptions;

        DECLARE_EVENT_TABLE()
};

#endif // EDITKEYWORDSDLG_H
