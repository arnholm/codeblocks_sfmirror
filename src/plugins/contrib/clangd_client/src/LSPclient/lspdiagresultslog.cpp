/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision: 63 $
 * $Id: lspdiagresultslog.cpp 63 2022-05-20 17:21:14Z pecanh $
 * $HeadURL: https://svn.code.sf.net/p/cb-clangd-client/code/trunk/clangd_client/src/LSPclient/lspdiagresultslog.cpp $
 */

//#include "sdk_precomp.h" gets not used because `EXPORT_LIB' not defined [-Winvalid-pch] error
#include "sdk.h"

#ifndef CB_PRECOMP
    #include <wx/arrstr.h>
    #include <wx/filename.h>
    #include <wx/listctrl.h>
    #include "manager.h"
    #include "editormanager.h"
    #include "cbeditor.h"
#endif
#include <wx/menu.h>
#include <wx/checklst.h>
#include "wx/xrc/xmlres.h"

#include <multiselectdlg.h>
#include <annoyingdialog.h>
#include <configmanager.h>

#include "cbstyledtextctrl.h"
#include "lspdiagresultslog.h"

namespace
{
    const int ID_List = wxNewId();
    const int idMenuIgnoredMsgs = wxNewId();
}

BEGIN_EVENT_TABLE(LSPDiagnosticsResultsLog, wxEvtHandler)
//
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
LSPDiagnosticsResultsLog::LSPDiagnosticsResultsLog(const wxArrayString& titles_in, wxArrayInt& widths_in,  wxArrayString& aIgnoredMsgs)
// ----------------------------------------------------------------------------
    : ListCtrlLogger(titles_in, widths_in),
      rUsrIgnoredDiagnostics(aIgnoredMsgs) //reference to client persistent wxArrayString of log ignored messages

{
    Connect(idMenuIgnoredMsgs, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &LSPDiagnosticsResultsLog::OnSetIgnoredMsgs);


}
// ----------------------------------------------------------------------------
wxEvtHandler* LSPDiagnosticsResultsLog::FindEventHandler(wxEvtHandler* pEvtHdlr)
// ----------------------------------------------------------------------------
{
    wxEvtHandler* pFoundEvtHdlr =  Manager::Get()->GetAppWindow()->GetEventHandler();

    while (pFoundEvtHdlr != nullptr)
    {
        if (pFoundEvtHdlr == pEvtHdlr)
            return pFoundEvtHdlr;
        pFoundEvtHdlr = pFoundEvtHdlr->GetNextHandler();
    }
    return nullptr;
}
// ----------------------------------------------------------------------------
LSPDiagnosticsResultsLog::~LSPDiagnosticsResultsLog()
// ----------------------------------------------------------------------------
{
    //dtor
    Disconnect(idMenuIgnoredMsgs, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &LSPDiagnosticsResultsLog::OnSetIgnoredMsgs);
    if (FindEventHandler(this))
        Manager::Get()->GetAppWindow()->RemoveEventHandler(this);   //(ph 2021/04/17)
}
// ----------------------------------------------------------------------------
wxWindow* LSPDiagnosticsResultsLog::CreateControl(wxWindow* parent)
// ----------------------------------------------------------------------------
{
    ListCtrlLogger::CreateControl(parent);
    control->SetId(ID_List);
    Connect(ID_List, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &LSPDiagnosticsResultsLog::OnDoubleClick);
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
    m_pControl = control;
    return control;
}

// ----------------------------------------------------------------------------
bool LSPDiagnosticsResultsLog::HasFeature(Feature::Enum feature) const
// ----------------------------------------------------------------------------
{
    if (feature == Feature::Additional)
        return true;
    else
        return ListCtrlLogger::HasFeature(feature);
}

// ----------------------------------------------------------------------------
void LSPDiagnosticsResultsLog::AppendAdditionalMenuItems(wxMenu &menu)
// ----------------------------------------------------------------------------
{
    menu.Append(idMenuIgnoredMsgs, _("Show/Set ignore messages"), _("Show/Set ignored messages"));
}

// ----------------------------------------------------------------------------
void LSPDiagnosticsResultsLog::FocusEntry(size_t index)
// ----------------------------------------------------------------------------
{
    if (index < (size_t)control->GetItemCount())
    {
        control->SetItemState(index, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
        control->EnsureVisible(index);
        //-SyncEditor(index);
    }
}

// ----------------------------------------------------------------------------
void LSPDiagnosticsResultsLog::SyncEditor(int selIndex)
// ----------------------------------------------------------------------------
{
    wxFileName filename(control->GetItemText(selIndex));
    wxString file;
    if (not filename.Exists()) return;
    if (!filename.IsAbsolute())
        filename.MakeAbsolute(m_Base);
    file = filename.GetFullPath();

    wxListItem li;
    li.m_itemId = selIndex;
    li.m_col = 1;
    li.m_mask = wxLIST_MASK_TEXT;
    control->GetItem(li);
    long line = 0;
    li.m_text.ToLong(&line);
    cbEditor* ed = Manager::Get()->GetEditorManager()->Open(file);
    if (!line || !ed)
        return;

    line -= 1;
    ed->Activate();
    ed->GotoLine(line);

    if (cbStyledTextCtrl* ctrl = ed->GetControl()) {
        ctrl->EnsureVisible(line);
    }
}

// ----------------------------------------------------------------------------
void LSPDiagnosticsResultsLog::OnDoubleClick(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // go to the relevant file/line
    if (control->GetSelectedItemCount() == 0)
        return;

    // find selected item index
    int index = control->GetNextItem(-1,
                                     wxLIST_NEXT_ALL,
                                     wxLIST_STATE_SELECTED);

    SyncEditor(index);
} // end of OnDoubleClick

// ----------------------------------------------------------------------------
void LSPDiagnosticsResultsLog::OnSetIgnoredMsgs(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // create dialog showing LSP ignored textDocument/publishDiagnostic messages
    // Log lines contain filename|lineNumber|msg
    //eg., F:\usr\Proj\Clangd_Client\plugin\plugins\LSPclient\lspdiagresultslog.cpp|16|note:In included file: definition of builtin function '__rdtsc'|


    wxString annoyingMsg = "Setting a diagnostic as ignored may cause the associated statement to also";
    annoyingMsg += "\nbe ignored for references, declarations, implementations etc.";
    AnnoyingDialog annoyingDlg("Set ignored log messages", annoyingMsg, wxART_INFORMATION,  AnnoyingDialog::OK);
    annoyingDlg.ShowModal();

    int cnt = GetItemsCount(); //log lines
    //-if (not cnt) return;
    wxArrayString aryOfLogItems;
    for (int ii=0; ii<cnt; ++ii)
    {
        wxString logItem = GetItemAsText(ii);
        if ( logItem.StartsWith("LSP:diagnostics") )  //skip Time stamped lines
        {
            // Use only the log items after the last time stamp separator line
            aryOfLogItems.Empty();
            continue;
        }
        logItem = logItem.BeforeLast('|').AfterLast('|');
        // verify msg not already in client persistent ignore array
        bool duplicate = false;
        for (size_t jj=0; jj<rUsrIgnoredDiagnostics.GetCount(); ++jj )
        {
            if ( rUsrIgnoredDiagnostics[jj] == logItem)
                duplicate = true;
        }
        if (not duplicate)
            aryOfLogItems.Add(logItem); //add item to client msgs ignore array
    }//endfor

    // Display log messages, then save user selections into client "ignore messages" array
    MultiSelectDlg dlg(Manager::Get()->GetAppWindow(), rUsrIgnoredDiagnostics, true);
    wxCheckListBox* pdlglst = XRCCTRL(dlg, "lstItems", wxCheckListBox);
    for (size_t ii=0; ii<aryOfLogItems.GetCount(); ++ii)
        pdlglst->Append(aryOfLogItems[ii]);
    if (dlg.ShowModal() == wxID_OK)
    {
        rUsrIgnoredDiagnostics.Empty();
        rUsrIgnoredDiagnostics = dlg.GetSelectedStrings();
        // write array of ignored messages to the config
        ConfigManager* pCfgMgr = Manager::Get()->GetConfigManager("clangd_client");
        pCfgMgr->Write("ignored_diagnostics", rUsrIgnoredDiagnostics);
    }
    return;
}
