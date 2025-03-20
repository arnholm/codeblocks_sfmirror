/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <wx/artprov.h>
    #include <wx/button.h>
    #include <wx/image.h>
    #include <wx/sizer.h>
    #include <wx/statbmp.h>
    #include <wx/stattext.h>

    #include <cbeditor.h>
    #include <cbproject.h>
    #include <editorcolourset.h>
    #include <editormanager.h>
    #include <logmanager.h>
#endif

#include <wx/progdlg.h>
#include <wx/xrc/xmlres.h>

#include <cbstyledtextctrl.h>
#include <encodingdetector.h>
#include <searchresultslog.h>

#include "coderefactoring.h"
#include "parsemanager.h"
#include "parser/parser.h"
#include "parser/cclogger.h"

#define CC_CODEREFACTORING_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_CODEREFACTORING_DEBUG_OUTPUT
        #define CC_CODEREFACTORING_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_CODEREFACTORING_DEBUG_OUTPUT
        #define CC_CODEREFACTORING_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_CODEREFACTORING_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...)
#elif CC_CODEREFACTORING_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                                              \
        do                                                                      \
        {                                                                       \
            if (g_EnableDebugTrace)                                             \
                CCLogger::Get()->DebugLog(wxString::Format(format, ##args));                   \
        }                                                                       \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
#else
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

// ----------------------------------------------------------------------------
class ScopeDialog : public wxDialog
// ----------------------------------------------------------------------------
{
public:
    ScopeDialog(wxWindow* parent, const wxString& title)
        // : wxDialog(parent, wxID_ANY, title)
        : wxDialog(parent, XRCID("ScopeDialog"), title)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* infoSizer = new wxBoxSizer(wxHORIZONTAL);
        const wxBitmap iconBmp = wxArtProvider::GetBitmap(wxT("core/find"), wxART_BUTTON);
        wxStaticBitmap* findIco = new wxStaticBitmap(this, wxID_ANY, iconBmp, wxDefaultPosition,
                                                     wxSize(iconBmp.GetWidth(),
                                                            iconBmp.GetHeight()));
        infoSizer->Add(findIco, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        wxStaticText* scopeText = new wxStaticText(this, wxID_ANY, _("Please choose the find scope for search tokens"));
        infoSizer->Add(scopeText, 1, wxALL | wxALIGN_CENTER_VERTICAL,
                       wxDLG_UNIT(this, wxSize(5, 0)).GetWidth());
        sizer->Add(infoSizer, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
        wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        m_OpenFiles = new wxButton(this, ID_OPEN_FILES, _("&Open files"), wxDefaultPosition, wxDefaultSize, 0,
                                   wxDefaultValidator, _T("ID_OPEN_FILES"));
        m_OpenFiles->SetDefault();
        btnSizer->Add(m_OpenFiles, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        m_ProjectFiles = new wxButton(this, ID_PROJECT_FILES, _("&Project files"), wxDefaultPosition,
                                      wxDefaultSize, 0, wxDefaultValidator, _T("ID_PROJECT_FILES"));
        btnSizer->Add(m_ProjectFiles, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        wxButton *closeButton = new wxButton(this, wxID_CANCEL, _("&Cancel"), wxDefaultPosition,
                                      wxDefaultSize);
        btnSizer->Add(closeButton, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        sizer->Add(btnSizer, 1, wxBOTTOM | wxLEFT | wxRIGHT | wxALIGN_CENTER_HORIZONTAL, 5);
        SetSizer(sizer);
        sizer->Fit(this);
        sizer->SetSizeHints(this);
        Center();

        Connect(ID_OPEN_FILES, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ScopeDialog::OnOpenFilesClick);
        Connect(ID_PROJECT_FILES, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ScopeDialog::OnProjectFilesClick);
        Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, (wxObjectEventFunction)&ScopeDialog::OnClose);
    }

public:
    static const long ID_OPEN_FILES;
    static const long ID_PROJECT_FILES;

private:
    void OnClose(cb_unused wxCloseEvent& event) { EndDialog(wxID_CLOSE); }
    void OnOpenFilesClick(cb_unused wxCommandEvent& event) { EndDialog(ID_OPEN_FILES);}
    void OnProjectFilesClick(cb_unused wxCommandEvent& event) { EndDialog(ID_PROJECT_FILES); }

    wxButton* m_OpenFiles;
    wxButton* m_ProjectFiles;
};

const long ScopeDialog::ID_OPEN_FILES = wxNewId();
const long ScopeDialog::ID_PROJECT_FILES = wxNewId();


// ----------------------------------------------------------------------------
CodeRefactoring::CodeRefactoring(ParseManager* pParseManager) :
// ----------------------------------------------------------------------------
    m_pParseManager(pParseManager)
{
}
// ----------------------------------------------------------------------------
CodeRefactoring::~CodeRefactoring()
// ----------------------------------------------------------------------------
{
}
// ----------------------------------------------------------------------------
wxString CodeRefactoring::GetSymbolUnderCursor()
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* editor = edMan->GetBuiltinActiveEditor();
    if (!editor)
        return wxEmptyString;

    cbStyledTextCtrl* control = editor->GetControl();
    const int style = control->GetStyleAt(control->GetCurrentPos());
    if (control->IsString(style) || control->IsComment(style))
        return wxEmptyString;

    if (!m_pParseManager->GetParser().Done())
    {
        wxString msg(_("The Parser is still parsing files."));
        cbMessageBox(msg, _("Code Refactoring"), wxOK | wxICON_WARNING);
        msg += m_pParseManager->GetParser().NotDoneReason();
        CCLogger::Get()->DebugLog(msg);

        return wxEmptyString;
    }

    const int pos = editor->GetControl()->GetCurrentPos();
    const int start = editor->GetControl()->WordStartPosition(pos, true);
    const int end = editor->GetControl()->WordEndPosition(pos, true);
    return editor->GetControl()->GetTextRange(start, end);
}
// ----------------------------------------------------------------------------
bool CodeRefactoring::Parse()
// ----------------------------------------------------------------------------
{
    cbThrow("CodeRefactoring::Parse() calling m_pParseManager->MarkItemsByAI()");
    return false;

}
// ----------------------------------------------------------------------------
void CodeRefactoring::RenameSymbols()
// ----------------------------------------------------------------------------
{

}

// ----------------------------------------------------------------------------
size_t CodeRefactoring::SearchInFiles(const wxArrayString& files, const wxString& targetText)
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    m_SearchDataMap.clear();

    // now that list is filled, we'll search
    wxWindow* parent = edMan->GetBuiltinActiveEditor()->GetParent();
    //cbStyledTextCtrl* control = new cbStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxSize(0, 0)); Dont eat up IDs
    cbStyledTextCtrl* control = new cbStyledTextCtrl(parent, XRCID("SearchInFilesEditor"), wxDefaultPosition, wxSize(0, 0));
    control->Show(false);

    // let's create a progress dialog because it might take some time depending on the files count
    wxProgressDialog* progress = new wxProgressDialog(_("Code Refactoring"),
                                                      _("Please wait while searching inside the project..."),
                                                      files.GetCount(),
                                                      Manager::Get()->GetAppWindow(),
                                                      wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_CAN_ABORT);
    PlaceWindow(progress);

    for (size_t i = 0; i < files.GetCount(); ++i)
    {
        // update the progress bar
        if (!progress->Update(i))
            break; // user pressed "Cancel"

        // check if the file is already opened in built-in editor and do search in it
        cbEditor* ed = edMan->IsBuiltinOpen(files[i]);
        if (ed)
            control->SetText(ed->GetControl()->GetText());
        else // else load the file in the control
        {
            EncodingDetector detector(files[i]);
            if (!detector.IsOK())
                continue; // failed
            control->SetText(detector.GetWxStr());
        }

        Find(control, files[i], targetText);
    }

    delete control; // done with it
    delete progress; // done here too

    return m_SearchDataMap.size();
}

size_t CodeRefactoring::VerifyResult(const TokenIdxSet& targetResult, const wxString& targetText,
                                     bool isLocalVariable)
{
    cbThrow("CodeRefactoring::VerifyResult() calling  m_pParseManager->MarkItemsByAI()");
    return 0;

}

void CodeRefactoring::Find(cbStyledTextCtrl* control, const wxString& file, const wxString& target)
{
    const int end = control->GetLength();
    int start = 0;

    for (;;)
    {
        int endPos;
        int pos = control->FindText(start, end, target, wxSCI_FIND_WHOLEWORD | wxSCI_FIND_MATCHCASE, &endPos);
        if (pos != wxSCI_INVALID_POSITION)
        {
            start = endPos;
            const int line = control->LineFromPosition(pos);
            wxString text = control->GetLine(line).Trim(true).Trim(false);
            m_SearchDataMap[file].push_back(crSearchData(pos, line + 1, text));
        }
        else
            break;
    }
}

// ----------------------------------------------------------------------------
void CodeRefactoring::DoFindReferences()
// ----------------------------------------------------------------------------
{
     /// Unused for clangd

    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!editor)
        return;

    cbSearchResultsLog* searchLog = Manager::Get()->GetSearchResultLogger();
    if (!searchLog)
        return;

    const wxString focusFile = editor->GetFilename();
    const int focusLine = editor->GetControl()->GetCurrentLine() + 1;
    wxFileName fn(focusFile);
    const wxString basePath(fn.GetPath());
    size_t index = 0;
    size_t focusIndex = 0;

    searchLog->Clear();
    searchLog->SetBasePath(basePath);

    for (SearchDataMap::iterator it = m_SearchDataMap.begin(); it != m_SearchDataMap.end(); ++it)
    {
        for (SearchDataList::iterator itList = it->second.begin(); itList != it->second.end(); ++itList)
        {
            if (it->first == focusFile && itList->line == focusLine)
                focusIndex = index;

            wxArrayString values;
            wxFileName curFn(it->first);
            curFn.MakeRelativeTo(basePath);
            values.Add(curFn.GetFullPath());
            values.Add(wxString::Format(_T("%d"), itList->line));
            values.Add(itList->text);
            searchLog->Append(values, Logger::info);

            ++index;
        }
    }

    if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_search"), true))
    {
        CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, searchLog);
        CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
        Manager::Get()->ProcessEvent(evtSwitch);
        Manager::Get()->ProcessEvent(evtShow);
    }

    searchLog->FocusEntry(focusIndex);
}

void CodeRefactoring::DoRenameSymbols(const wxString& targetText, const wxString& replaceText)
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* editor = edMan->GetBuiltinActiveEditor();
    if (!editor)
        return;

    cbProject* project = m_pParseManager->GetProjectByEditor(editor);
    for (SearchDataMap::iterator it = m_SearchDataMap.begin(); it != m_SearchDataMap.end(); ++it)
    {
        // check if the file is already opened in built-in editor and do search in it
        cbEditor* ed = edMan->IsBuiltinOpen(it->first);
        if (!ed)
        {
            ProjectFile* pf = project ? project->GetFileByFilename(it->first) : 0;
            ed = edMan->Open(it->first, it->second.front().pos, pf);
        }

        cbStyledTextCtrl* control = ed->GetControl();
        control->BeginUndoAction();

        for (SearchDataList::reverse_iterator rIter = it->second.rbegin(); rIter != it->second.rend(); ++rIter)
        {
            control->SetTargetStart(rIter->pos);
            control->SetTargetEnd(rIter->pos + targetText.Len());
            control->ReplaceTarget(replaceText);
            // for find references
            rIter->text.Replace(targetText, replaceText);
        }

        control->EndUndoAction();
    }
}

void CodeRefactoring::GetAllProjectFiles(wxArrayString& files, cbProject* project)
{
    if (!project)
        return;

    // fill the search list with all the project files
    for (FilesList::const_iterator it = project->GetFilesList().begin();
                                   it != project->GetFilesList().end(); ++it)
    {
        ProjectFile* pf = *it;
        if (!pf)
            continue;

        ParserCommon::EFileType ft = ParserCommon::FileType(pf->relativeFilename);
        if (ft != ParserCommon::ftOther)
            files.Add(pf->file.GetFullPath());
    }
}

void CodeRefactoring::GetOpenedFiles(wxArrayString& files)
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    if (edMan)
    {
        for (int i = 0; i < edMan->GetEditorsCount(); ++i)
            files.Add(edMan->GetEditor(i)->GetFilename());
    }
}
