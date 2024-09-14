/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CCDEBUGINFO_H
#define CCDEBUGINFO_H

#include <wx/wxprec.h>

//(*Headers(CCDebugInfo)
#include "scrollingdialog.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

class ParserBase;
class Token;

// When instance this class, *MUST* entered a critical section, and should call ShowModal() rather than the other
// e.g. wxMutexLocker locker(s_TokenTreeMutex);
class CCDebugInfo: public wxScrollingDialog
{
public:
    CCDebugInfo(wxWindow* parent, ParserBase* parser, Token* token);
    virtual ~CCDebugInfo();

    void FillFiles();
    void FillDirs();
    void FillMacros();
    void DisplayTokenInfo();
    void FillChildren();
    void FillAncestors();
    void FillDescendants();

    //(*Identifiers(CCDebugInfo)
    static const wxWindowID ID_TEXTCTRL1;
    static const wxWindowID ID_BUTTON1;
    static const wxWindowID ID_STATICTEXT18;
    static const wxWindowID ID_STATICTEXT2;
    static const wxWindowID ID_STATICTEXT10;
    static const wxWindowID ID_STATICTEXT12;
    static const wxWindowID ID_STATICTEXT4;
    static const wxWindowID ID_STATICTEXT6;
    static const wxWindowID ID_STATICTEXT8;
    static const wxWindowID ID_STATICTEXT37;
    static const wxWindowID ID_STATICTEXT41;
    static const wxWindowID ID_STATICTEXT14;
    static const wxWindowID ID_STATICTEXT16;
    static const wxWindowID ID_STATICTEXT33;
    static const wxWindowID ID_STATICTEXT39;
    static const wxWindowID ID_STATICTEXT1;
    static const wxWindowID ID_STATICTEXT20;
    static const wxWindowID ID_STATICTEXT24;
    static const wxWindowID ID_BUTTON4;
    static const wxWindowID ID_COMBOBOX3;
    static const wxWindowID ID_BUTTON5;
    static const wxWindowID ID_COMBOBOX2;
    static const wxWindowID ID_BUTTON3;
    static const wxWindowID ID_COMBOBOX1;
    static const wxWindowID ID_BUTTON2;
    static const wxWindowID ID_STATICTEXT26;
    static const wxWindowID ID_BUTTON7;
    static const wxWindowID ID_STATICTEXT28;
    static const wxWindowID ID_BUTTON8;
    static const wxWindowID ID_STATICTEXT35;
    static const wxWindowID ID_PANEL1;
    static const wxWindowID ID_LISTBOX1;
    static const wxWindowID ID_PANEL2;
    static const wxWindowID ID_LISTBOX2;
    static const wxWindowID ID_PANEL3;
    static const wxWindowID ID_LISTBOX3;
    static const wxWindowID ID_PANEL4;
    static const wxWindowID ID_NOTEBOOK1;
    static const wxWindowID ID_BUTTON6;
    //*)

protected:
    //(*Handlers(CCDebugInfo)
    void OnInit(wxInitDialogEvent& event);
    void OnFindClick(wxCommandEvent& event);
    void OnGoAscClick(wxCommandEvent& event);
    void OnGoDescClick(wxCommandEvent& event);
    void OnGoParentClick(wxCommandEvent& event);
    void OnGoChildrenClick(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnGoDeclClick(wxCommandEvent& event);
    void OnGoImplClick(wxCommandEvent& event);
    //*)

    //(*Declarations(CCDebugInfo)
    wxBoxSizer* BoxSizer11;
    wxBoxSizer* BoxSizer12;
    wxBoxSizer* BoxSizer5;
    wxButton* btnSave;
    wxComboBox* cmbAncestors;
    wxComboBox* cmbChildren;
    wxComboBox* cmbDescendants;
    wxListBox* lstDirs;
    wxListBox* lstFiles;
    wxListBox* lstMacros;
    wxPanel* Panel4;
    wxStaticText* txtArgs;
    wxStaticText* txtArgsStripped;
    wxStaticText* txtBaseType;
    wxStaticText* txtDeclFile;
    wxStaticText* txtFullType;
    wxStaticText* txtID;
    wxStaticText* txtImplFile;
    wxStaticText* txtInfo;
    wxStaticText* txtIsConst;
    wxStaticText* txtIsLocal;
    wxStaticText* txtIsNoExcept;
    wxStaticText* txtIsOp;
    wxStaticText* txtIsTemp;
    wxStaticText* txtKind;
    wxStaticText* txtName;
    wxStaticText* txtNamespace;
    wxStaticText* txtParent;
    wxStaticText* txtScope;
    wxStaticText* txtTemplateArg;
    wxStaticText* txtUserData;
    wxTextCtrl* txtFilter;
    //*)

private:
    ParserBase* m_Parser;
    Token*      m_Token;

    DECLARE_EVENT_TABLE()
};

#endif
