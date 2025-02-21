/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#include "main.h"

#include "annoyingdialog.h"
#include "app.h"
#include "appglobals.h"
#include "batchbuild.h"
#include "cbart_provider.h"
#include "cbauibook.h"
#include "cbcolourmanager.h"
#include "cbexception.h"
#include "cbplugin.h"
#include "cbproject.h"
#include "cbstatusbar.h"
#include "cbstyledtextctrl.h"
#include "cbworkspace.h"
#include "ccmanager.h"
#include "compilersettingsdlg.h"
#include "configmanager.h"
#include "debugger_interface_creator.h"
#include "debuggermanager.h"
#include "debuggermenu.h"
#include "debuggersettingsdlg.h"
#include "dlgabout.h"
#include "dlgaboutplugin.h"
#include "editorcolourset.h"
#include "editorconfigurationdlg.h"
#include "editormanager.h"
#include "environmentsettingsdlg.h"
#include "filefilters.h"
#include "globals.h"
#include "infopane.h"
#include "infowindow.h"
#include "loggers.h"
#include "logmanager.h"
#include "notebookstyles.h"
#include "personalitymanager.h"
#include "pluginmanager.h"
#include "printdlg.h"
#include "projectmanager.h"
#include "projectmanagerui.h"
#include "scriptconsole.h"
#include "scriptingmanager.h"
#include "scriptingsettingsdlg.h"
#include "sdk_events.h"
#include "startherepage.h"
#include "switcherdlg.h"
#include "templatemanager.h"
#include "toolsmanager.h"
#include "uservarmanager.h"

#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/printdlg.h>
#include <wx/sstream.h>
#include <wx/tipdlg.h>
#include <wx/tokenzr.h>
#include <wx/xrc/xmlres.h>

#include "scripting/bindings/sc_utils.h"
#include "scripting/bindings/sc_typeinfo_all.h"

class cbFileDropTarget : public wxFileDropTarget
{
public:
    cbFileDropTarget(MainFrame *frame):m_frame(frame){}
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override
    {
        if (!m_frame) return false;
        return m_frame->OnDropFiles(x,y,filenames);
    }
private:
    MainFrame* m_frame;
};

const int idHighlightButton = wxNewId();

struct MainStatusBar : cbStatusBar
{
    static const int numFields = 9;

    MainStatusBar(wxWindow* parent,  wxWindowID id, long style, const wxString& name) : cbStatusBar(parent, id, style, name)
    {
        Bind(wxEVT_SIZE, &MainStatusBar::OnSize, this);
    }

    void CreateAndFill()
    {
        int h;
        size_t num = 0;

        wxCoord widths[16]; // 16 max
        widths[num++] = -1; // main field

        wxClientDC dc(this);
        dc.GetTextExtent(_(" Highlight Button "),                &widths[num++], &h);
        dc.GetTextExtent(_(" Windows (CR+LF) "),                 &widths[num++], &h);
        dc.GetTextExtent(_(" WINDOWS-1252 "),                    &widths[num++], &h);
        dc.GetTextExtent(_(" Line 12345, Col 123, Pos 123456 "), &widths[num++], &h);
        dc.GetTextExtent(_(" Overwrite "),                       &widths[num++], &h);
        dc.GetTextExtent(_(" Modified "),                        &widths[num++], &h);
        dc.GetTextExtent(_(" Read/Write "),                      &widths[num++], &h);
        dc.GetTextExtent(_(" name_of_profile "),                 &widths[num++], &h);

        SetFieldsCount(num);
        SetStatusWidths(num, widths);

        // Highlight button
        {
            m_pHighlightButton = new wxButton(this, idHighlightButton, "bla", wxDefaultPosition, wxDefaultSize,
                                              wxBORDER_NONE|wxBU_LEFT|wxBU_EXACTFIT);
            m_pHighlightButton->Disable();
            m_pHighlightButton->Hide();
            // Adjust status bar height to fit the button.
            // This affects wx3.x build more than wx2.8 builds. At least on wxGTK.
            const int height = std::max(GetMinHeight(), m_pHighlightButton->GetClientSize().GetHeight());
            SetMinHeight(height);
        }

        SetStatusText(wxString::Format(_("Welcome to %s!"), appglobals::AppName));
        SetStatusText(wxString(), 1);
    }

    void UpdateFields()
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        wxString personality(Manager::Get()->GetPersonalityManager()->GetPersonality());
        if (ed)
        {
            cbStyledTextCtrl * const control = ed->GetControl();

            int panel = 0;
            int pos = control->GetCurrentPos();
            wxString msg;
            SetStatusText(ed->GetFilename(), panel++);

            if (m_pHighlightButton)
            {
                EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
                if (colour_set)
                    ChangeButtonLabel(*m_pHighlightButton, colour_set->GetLanguageName(ed->GetLanguage()));
                else
                    ChangeButtonLabel(*m_pHighlightButton, wxString());
            }
            // EOL mode
            panel++;
            switch (control->GetEOLMode())
            {
                case wxSCI_EOL_CRLF: msg = "Windows (CR+LF)"; break;
                case wxSCI_EOL_CR:   msg = "Mac (CR)";        break;
                case wxSCI_EOL_LF:   msg = "Unix (LF)";       break;
                default:                                      break;
            }
            SetStatusText(msg, panel++);
            SetStatusText(ed->GetEncodingName(), panel++);
            msg.Printf(_("Line %d, Col %d, Pos %d"), control->GetCurrentLine() + 1, control->GetColumn(pos) + 1, pos);
            SetStatusText(msg, panel++);
            SetStatusText(control->GetOvertype() ? _("Overwrite") : _("Insert"), panel++);
            SetStatusText(ed->GetModified() ? _("Modified") : wxString(), panel++);
            SetStatusText(control->GetReadOnly() ? _("Read only") : _("Read/Write"), panel++);
            SetStatusText(personality, panel++);
        }
        else
        {
            int panel = 0;
            EditorBase *eb = Manager::Get()->GetEditorManager()->GetActiveEditor();
            if ( eb )
                SetStatusText(eb->GetFilename(), panel++);
            else
                SetStatusText(wxString::Format(_("Welcome to %s!"), appglobals::AppName), panel++);

            if (m_pHighlightButton)
                ChangeButtonLabel(*m_pHighlightButton, wxString());
            panel++;

            SetStatusText(wxString(), panel++);
            SetStatusText(wxString(), panel++);
            SetStatusText(wxString(), panel++);
            SetStatusText(wxString(), panel++);
            SetStatusText(wxString(), panel++);
            SetStatusText(wxString(), panel++);
            SetStatusText(personality, panel++);
        }
    }
private:
    /// Change the label of a button only if it has really changed. This is used for status bar
    /// button, because if we always set the label there is flickering while scrolling in the
    /// editor.I've observed the flickering on wxGTK and I don't know if it is present on the other
    /// ports.
    static void ChangeButtonLabel(wxButton &button, const wxString &text)
    {
        if (text != button.GetLabel())
            button.SetLabel(text);
        if (!button.IsEnabled() && !text.empty())
        {
            button.Enable();
            button.Show();
        }
        if (button.IsEnabled() && text.empty())
        {
            button.Hide();
            button.Disable();
        }
    }

    void OnSize(wxSizeEvent &event)
    {
        AdjustFieldsSize();

        // for flicker-free display
        event.Skip();
    }

    void AdjustFieldsSize() override
    {
        cbStatusBar::AdjustFieldsSize();
        if (m_pHighlightButton)
        {
            wxRect rect;
            if (GetFieldRect(1, rect))
            {
                m_pHighlightButton->SetPosition(rect.GetPosition());
                m_pHighlightButton->SetSize(rect.GetSize());
            }
        }
    }
private:
    wxButton *m_pHighlightButton = nullptr;
};

const static wxString gDefaultLayout = "Code::Blocks default";
static wxString gDefaultLayoutData; // this will keep the "hardcoded" default layout
static wxString gDefaultMessagePaneLayoutData; // this will keep default layout

const static wxString gMinimalLayout = "Code::Blocks minimal";
static wxString gMinimalLayoutData; // this will keep the "hardcoded" default layout
static wxString gMinimalMessagePaneLayoutData; // this will keep default layout

// In <wx/defs.h> wxID_FILE[X] exists only from 1..9,
// so create our own here with a *continuous* numbering!
// The wxID_FILE[X] enum usually starts at 5050 until 5059,
// followed by wxID_OK starting at 5100. (wxWidgets v2.6, v2.8 and v2.9)
// so we use the space in between starting from 5060
// and hoping it doesn't change too much in <wx/defs.h> ;-)
enum
{
    wxID_CBFILE01   = 5060, // Recent files...
    wxID_CBFILE02, // 5061
    wxID_CBFILE03, // 5062
    wxID_CBFILE04, // 5063
    wxID_CBFILE05, // 5064
    wxID_CBFILE06, // 5065
    wxID_CBFILE07, // 5066
    wxID_CBFILE08, // 5067
    wxID_CBFILE09, // 5068
    wxID_CBFILE10, // 5069
    wxID_CBFILE11, // 5070
    wxID_CBFILE12, // 5071
    wxID_CBFILE13, // 5072
    wxID_CBFILE14, // 5073
    wxID_CBFILE15, // 5074
    wxID_CBFILE16, // 5075
    wxID_CBFILE17, // 5076  // Starting here for recent projects...
    wxID_CBFILE18, // 5077
    wxID_CBFILE19, // 5078
    wxID_CBFILE20, // 5079
    wxID_CBFILE21, // 5080
    wxID_CBFILE22, // 5081
    wxID_CBFILE23, // 5082
    wxID_CBFILE24, // 5083
    wxID_CBFILE25, // 5084
    wxID_CBFILE26, // 5085
    wxID_CBFILE27, // 5086
    wxID_CBFILE28, // 5087
    wxID_CBFILE29, // 5088
    wxID_CBFILE30, // 5089
    wxID_CBFILE31, // 5090
    wxID_CBFILE32  // 5091
};

int idToolNew                           = XRCID("idToolNew");
int idFileNew                           = XRCID("idFileNew");
int idFileNewEmpty                      = XRCID("idFileNewEmpty");
int idFileNewProject                    = XRCID("idFileNewProject");
int idFileNewTarget                     = XRCID("idFileNewTarget");
int idFileNewFile                       = XRCID("idFileNewFile");
int idFileNewCustom                     = XRCID("idFileNewCustom");
int idFileNewUser                       = XRCID("idFileNewUser");
int idFileOpen                          = XRCID("idFileOpen");
int idFileReopen                        = XRCID("idFileReopen");
int idFileOpenRecentFileClearHistory    = XRCID("idFileOpenRecentFileClearHistory");
int idFileOpenRecentProjectClearHistory = XRCID("idFileOpenRecentProjectClearHistory");
int idFileImportProjectDevCpp           = XRCID("idFileImportProjectDevCpp");
int idFileImportProjectMSVC             = XRCID("idFileImportProjectMSVC");
int idFileImportProjectMSVCWksp         = XRCID("idFileImportProjectMSVCWksp");
int idFileImportProjectMSVS             = XRCID("idFileImportProjectMSVS");
int idFileImportProjectMSVSWksp         = XRCID("idFileImportProjectMSVSWksp");
int idFileSave                          = XRCID("idFileSave");
int idFileSaveAs                        = XRCID("idFileSaveAs");
int idFileReopenProject                 = XRCID("idFileReopenProject");
int idFileSaveProject                   = XRCID("idFileSaveProject");
int idFileSaveProjectAs                 = XRCID("idFileSaveProjectAs");
int idFileSaveProjectTemplate           = XRCID("idFileSaveProjectTemplate");
int idFileOpenDefWorkspace              = XRCID("idFileOpenDefWorkspace");
int idFileSaveWorkspace                 = XRCID("idFileSaveWorkspace");
int idFileSaveWorkspaceAs               = XRCID("idFileSaveWorkspaceAs");
int idFileSaveAll                       = XRCID("idFileSaveAll");
int idFileCloseWorkspace                = XRCID("idFileCloseWorkspace");
int idFileClose                         = XRCID("idFileClose");
int idFileCloseAll                      = XRCID("idFileCloseAll");
int idFileCloseProject                  = XRCID("idFileCloseProject");
int idFilePrintSetup                    = XRCID("idFilePrintSetup");
int idFilePrint                         = XRCID("idFilePrint");
int idFileExit                          = XRCID("idFileExit");

int idEditUndo                    = XRCID("idEditUndo");
int idEditRedo                    = XRCID("idEditRedo");
int idEditClearHistory            = XRCID("idEditClearHistory");
int idEditCopy                    = XRCID("idEditCopy");
int idEditCut                     = XRCID("idEditCut");
int idEditPaste                   = XRCID("idEditPaste");
int idEditSwapHeaderSource        = XRCID("idEditSwapHeaderSource");
int idEditGotoMatchingBrace       = XRCID("idEditGotoMatchingBrace");
int idEditHighlightMode           = XRCID("idEditHighlightMode");
int idEditHighlightModeText       = XRCID("idEditHighlightModeText");
int idEditBookmarks               = XRCID("idEditBookmarks");
int idEditBookmarksToggle         = XRCID("idEditBookmarksToggle");
int idEditBookmarksPrevious       = XRCID("idEditBookmarksPrevious");
int idEditBookmarksNext           = XRCID("idEditBookmarksNext");
int idEditBookmarksClearAll       = XRCID("idEditBookmarksClearAll");
int idEditFolding                 = XRCID("idEditFolding");
int idEditFoldAll                 = XRCID("idEditFoldAll");
int idEditUnfoldAll               = XRCID("idEditUnfoldAll");
int idEditToggleAllFolds          = XRCID("idEditToggleAllFolds");
int idEditFoldBlock               = XRCID("idEditFoldBlock");
int idEditUnfoldBlock             = XRCID("idEditUnfoldBlock");
int idEditToggleFoldBlock         = XRCID("idEditToggleFoldBlock");
int idEditEOLMode                 = XRCID("idEditEOLMode");
int idEditEOLCRLF                 = XRCID("idEditEOLCRLF");
int idEditEOLCR                   = XRCID("idEditEOLCR");
int idEditEOLLF                   = XRCID("idEditEOLLF");
int idEditEncoding                = XRCID("idEditEncoding");
int idEditEncodingDefault         = XRCID("idEditEncodingDefault");
int idEditEncodingUseBom          = XRCID("idEditEncodingUseBom");
int idEditEncodingAscii           = XRCID("idEditEncodingAscii");
int idEditEncodingUtf7            = XRCID("idEditEncodingUtf7");
int idEditEncodingUtf8            = XRCID("idEditEncodingUtf8");
int idEditEncodingUnicode         = XRCID("idEditEncodingUnicode");
int idEditEncodingUtf16           = XRCID("idEditEncodingUtf16");
int idEditEncodingUtf32           = XRCID("idEditEncodingUtf32");
int idEditEncodingUnicode16BE     = XRCID("idEditEncodingUnicode16BE");
int idEditEncodingUnicode16LE     = XRCID("idEditEncodingUnicode16LE");
int idEditEncodingUnicode32BE     = XRCID("idEditEncodingUnicode32BE");
int idEditEncodingUnicode32LE     = XRCID("idEditEncodingUnicode32LE");
int idEditSpecialCommands         = XRCID("idEditSpecialCommands");
int idEditSpecialCommandsMovement = XRCID("idEditSpecialCommandsMovement");
int idEditParaUp                  = XRCID("idEditParaUp");
int idEditParaUpExtend            = XRCID("idEditParaUpExtend");
int idEditParaDown                = XRCID("idEditParaDown");
int idEditParaDownExtend          = XRCID("idEditParaDownExtend");
int idEditWordPartLeft            = XRCID("idEditWordPartLeft");
int idEditWordPartLeftExtend      = XRCID("idEditWordPartLeftExtend");
int idEditWordPartRight           = XRCID("idEditWordPartRight");
int idEditWordPartRightExtend     = XRCID("idEditWordPartRightExtend");
int idEditSpecialCommandsZoom     = XRCID("idEditSpecialCommandsZoom");
int idEditZoomIn                  = XRCID("idEditZoomIn");
int idEditZoomOut                 = XRCID("idEditZoomOut");
int idEditZoomReset               = XRCID("idEditZoomReset");
int idEditSpecialCommandsLine     = XRCID("idEditSpecialCommandsLine");
int idEditLineCut                 = XRCID("idEditLineCut");
int idEditLineDelete              = XRCID("idEditLineDelete");
int idEditLineDuplicate           = XRCID("idEditLineDuplicate");
int idEditLineTranspose           = XRCID("idEditLineTranspose");
int idEditLineCopy                = XRCID("idEditLineCopy");
int idEditLinePaste               = XRCID("idEditLinePaste");
int idEditLineUp                  = XRCID("idEditLineUp");
int idEditLineDown                = XRCID("idEditLineDown");
int idEditSpecialCommandsCase     = XRCID("idEditSpecialCommandsCase");
int idEditUpperCase               = XRCID("idEditUpperCase");
int idEditLowerCase               = XRCID("idEditLowerCase");
int idEditSpecialCommandsOther    = XRCID("idEditSpecialCommandsOther");
int idEditInsertNewLine           = XRCID("idEditInsertNewLine");
int idEditGotoLineEnd             = XRCID("idEditGotoLineEnd");
int idEditInsertNewLineBelow      = XRCID("idEditInsertNewLineBelow");
int idEditInsertNewLineAbove      = XRCID("idEditInsertNewLineAbove");
int idEditSelectAll               = XRCID("idEditSelectAll");
int idEditSelectNext              = XRCID("idEditSelectNext");
int idEditSelectNextSkip          = XRCID("idEditSelectNextSkip");
int idEditCommentSelected         = XRCID("idEditCommentSelected");
int idEditUncommentSelected       = XRCID("idEditUncommentSelected");
int idEditToggleCommentSelected   = XRCID("idEditToggleCommentSelected");
int idEditStreamCommentSelected   = XRCID("idEditStreamCommentSelected");
int idEditBoxCommentSelected      = XRCID("idEditBoxCommentSelected");
int idEditShowCallTip             = XRCID("idEditShowCallTip");
int idEditCompleteCode            = wxNewId();

int idViewLayoutDelete       = XRCID("idViewLayoutDelete");
int idViewLayoutSave         = XRCID("idViewLayoutSave");
int idViewToolbars           = XRCID("idViewToolbars");
int idViewToolFit            = XRCID("idViewToolFit");
int idViewToolOptimize       = XRCID("idViewToolOptimize");
int idViewToolMain           = XRCID("idViewToolMain");
int idViewToolDebugger       = XRCID("idViewToolDebugger");
int idViewManager            = XRCID("idViewManager");
int idViewLogManager         = XRCID("idViewLogManager");
int idViewStartPage          = XRCID("idViewStartPage");
int idViewStatusbar          = XRCID("idViewStatusbar");
int idViewScriptConsole      = XRCID("idViewScriptConsole");
int idViewHideEditorTabs     = XRCID("idViewHideEditorTabs");
int idViewFocusEditor        = XRCID("idViewFocusEditor");
int idViewFocusManagement    = XRCID("idViewFocusManagement");
int idViewFocusLogsAndOthers = XRCID("idViewFocusLogsAndOthers");
int idViewSwitchTabs         = XRCID("idViewSwitchTabs");
int idViewFullScreen         = XRCID("idViewFullScreen");

int idSearchFind                    = XRCID("idSearchFind");
int idSearchFindInFiles             = XRCID("idSearchFindInFiles");
int idSearchFindNext                = XRCID("idSearchFindNext");
int idSearchFindPrevious            = XRCID("idSearchFindPrevious");
int idSearchFindSelectedNext        = XRCID("idSearchFindSelectedNext");
int idSearchFindSelectedPrevious    = XRCID("idSearchFindSelectedPrevious");
int idSearchReplace                 = XRCID("idSearchReplace");
int idSearchReplaceInFiles          = XRCID("idSearchReplaceInFiles");
int idSearchGotoLine                = XRCID("idSearchGotoLine");
int idSearchGotoNextChanged         = XRCID("idSearchGotoNextChanged");
int idSearchGotoPreviousChanged     = XRCID("idSearchGotoPreviousChanged");

int idSettingsEnvironment    = XRCID("idSettingsEnvironment");
int idSettingsGlobalUserVars = XRCID("idSettingsGlobalUserVars");
int idSettingsBackticks      = XRCID("idSettingsBackticks");
int idSettingsEditor         = XRCID("idSettingsEditor");
int idSettingsCompiler       = XRCID("idSettingsCompiler");
int idSettingsDebugger       = XRCID("idSettingsDebugger");
int idPluginsManagePlugins   = XRCID("idPluginsManagePlugins");
int idSettingsScripting      = XRCID("idSettingsScripting");

int idHelpTips    = XRCID("idHelpTips");
int idHelpPlugins = XRCID("idHelpPlugins");

int idLeftSash              = XRCID("idLeftSash");
int idBottomSash            = XRCID("idBottomSash");
int idCloseFullScreen       = XRCID("idCloseFullScreen");

int idGetGlobalAccels   = XRCID("idGetGlobalAccels");

int idFileNext              = wxNewId();
int idFilePrev              = wxNewId();
int idShiftTab              = wxNewId();
int idCtrlAltTab            = wxNewId();
int idStartHerePageLink     = wxNewId();

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_ERASE_BACKGROUND(MainFrame::OnEraseBackground)
    EVT_CLOSE(MainFrame::OnApplicationClose)

    EVT_UPDATE_UI(idFileOpen,                          MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileOpenRecentFileClearHistory,    MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileOpenRecentProjectClearHistory, MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSave,                          MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveAs,                        MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileOpenDefWorkspace,              MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveWorkspace,                 MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveWorkspaceAs,               MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileCloseWorkspace,                MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileClose,                         MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileCloseAll,                      MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFilePrintSetup,                    MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFilePrint,                         MainFrame::OnFileMenuUpdateUI)

    EVT_UPDATE_UI(idFileReopenProject,          MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveProject,            MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveProjectAs,          MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveProjectTemplate,    MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileSaveAll,                MainFrame::OnFileMenuUpdateUI)
    EVT_UPDATE_UI(idFileCloseProject,           MainFrame::OnFileMenuUpdateUI)

    EVT_UPDATE_UI(idEditUndo,                  MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditRedo,                  MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditClearHistory,          MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditCopy,                  MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditCut,                   MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditPaste,                 MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditSwapHeaderSource,      MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditGotoMatchingBrace,     MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditFoldAll,               MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditUnfoldAll,             MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditToggleAllFolds,        MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditFoldBlock,             MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditUnfoldBlock,           MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditToggleFoldBlock,       MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditEOLCRLF,               MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditEOLCR,                 MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditEOLLF,                 MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditEncoding,              MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditSelectAll,             MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditSelectNext,            MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditSelectNextSkip,        MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksToggle,       MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksNext,         MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksPrevious,     MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksClearAll,     MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditCommentSelected,       MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditUncommentSelected,     MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditToggleCommentSelected, MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditStreamCommentSelected, MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditBoxCommentSelected,    MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditShowCallTip,           MainFrame::OnEditMenuUpdateUI)
    EVT_UPDATE_UI(idEditCompleteCode,          MainFrame::OnEditMenuUpdateUI)

    EVT_UPDATE_UI(idSearchFind,                 MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchFindInFiles,          MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchFindNext,             MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchFindPrevious,         MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchFindSelectedNext,     MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchFindSelectedPrevious, MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchReplace,              MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchReplaceInFiles,       MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchGotoLine,             MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchGotoNextChanged,      MainFrame::OnSearchMenuUpdateUI)
    EVT_UPDATE_UI(idSearchGotoPreviousChanged,  MainFrame::OnSearchMenuUpdateUI)

    EVT_UPDATE_UI(idViewToolMain,           MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewLogManager,         MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewStartPage,          MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewManager,            MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewStatusbar,          MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewScriptConsole,      MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewHideEditorTabs,     MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewFocusEditor,        MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewFocusManagement,    MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewFocusLogsAndOthers, MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewFullScreen,         MainFrame::OnViewMenuUpdateUI)
    EVT_UPDATE_UI(idViewToolDebugger,       MainFrame::OnViewMenuUpdateUI)

    EVT_UPDATE_UI(idEditHighlightModeText, MainFrame::OnEditHighlightModeUpdateUI)

    EVT_MENU(idFileNewEmpty,   MainFrame::OnFileNewWhat)
    EVT_MENU(idFileNewProject, MainFrame::OnFileNewWhat)
    EVT_MENU(idFileNewTarget,  MainFrame::OnFileNewWhat)
    EVT_MENU(idFileNewFile,    MainFrame::OnFileNewWhat)
    EVT_MENU(idFileNewCustom,  MainFrame::OnFileNewWhat)
    EVT_MENU(idFileNewUser,    MainFrame::OnFileNewWhat)

    EVT_MENU(idToolNew,                           MainFrame::OnFileNew)
    EVT_MENU(idFileOpen,                          MainFrame::OnFileOpen)
    EVT_MENU(idFileOpenRecentProjectClearHistory, MainFrame::OnFileOpenRecentProjectClearHistory)
    EVT_MENU(idFileOpenRecentFileClearHistory,    MainFrame::OnFileOpenRecentClearHistory)
    EVT_MENU_RANGE(wxID_CBFILE01, wxID_CBFILE16,  MainFrame::OnFileReopen)
    EVT_MENU_RANGE(wxID_CBFILE17, wxID_CBFILE32,  MainFrame::OnFileReopenProject)
    EVT_MENU(idFileImportProjectDevCpp,           MainFrame::OnFileImportProjectDevCpp)
    EVT_MENU(idFileImportProjectMSVC,             MainFrame::OnFileImportProjectMSVC)
    EVT_MENU(idFileImportProjectMSVCWksp,         MainFrame::OnFileImportProjectMSVCWksp)
    EVT_MENU(idFileImportProjectMSVS,             MainFrame::OnFileImportProjectMSVS)
    EVT_MENU(idFileImportProjectMSVSWksp,         MainFrame::OnFileImportProjectMSVSWksp)
    EVT_MENU(idFileSave,                          MainFrame::OnFileSave)
    EVT_MENU(idFileSaveAs,                        MainFrame::OnFileSaveAs)
    EVT_MENU(idFileSaveProject,                   MainFrame::OnFileSaveProject)
    EVT_MENU(idFileSaveProjectAs,                 MainFrame::OnFileSaveProjectAs)
    EVT_MENU(idFileSaveProjectTemplate,           MainFrame::OnFileSaveProjectTemplate)
    EVT_MENU(idFileOpenDefWorkspace,              MainFrame::OnFileOpenDefWorkspace)
    EVT_MENU(idFileSaveWorkspace,                 MainFrame::OnFileSaveWorkspace)
    EVT_MENU(idFileSaveWorkspaceAs,               MainFrame::OnFileSaveWorkspaceAs)
    EVT_MENU(idFileSaveAll,                       MainFrame::OnFileSaveAll)
    EVT_MENU(idFileCloseWorkspace,                MainFrame::OnFileCloseWorkspace)
    EVT_MENU(idFileClose,                         MainFrame::OnFileClose)
    EVT_MENU(idFileCloseAll,                      MainFrame::OnFileCloseAll)
    EVT_MENU(idFileCloseProject,                  MainFrame::OnFileCloseProject)
    EVT_MENU(idFilePrint,                         MainFrame::OnFilePrint)
    EVT_MENU(idFileExit,                          MainFrame::OnFileQuit)
    EVT_MENU(idFileNext,                          MainFrame::OnFileNext)
    EVT_MENU(idFilePrev,                          MainFrame::OnFilePrev)

    EVT_MENU(idEditUndo,                  MainFrame::OnEditUndo)
    EVT_MENU(idEditRedo,                  MainFrame::OnEditRedo)
    EVT_MENU(idEditClearHistory,          MainFrame::OnEditClearHistory)
    EVT_MENU(idEditCopy,                  MainFrame::OnEditCopy)
    EVT_MENU(idEditCut,                   MainFrame::OnEditCut)
    EVT_MENU(idEditPaste,                 MainFrame::OnEditPaste)
    EVT_MENU(idEditSwapHeaderSource,      MainFrame::OnEditSwapHeaderSource)
    EVT_MENU(idEditGotoMatchingBrace,     MainFrame::OnEditGotoMatchingBrace)
    EVT_MENU(idEditHighlightModeText,     MainFrame::OnEditHighlightMode)
    EVT_MENU(idEditFoldAll,               MainFrame::OnEditFoldAll)
    EVT_MENU(idEditUnfoldAll,             MainFrame::OnEditUnfoldAll)
    EVT_MENU(idEditToggleAllFolds,        MainFrame::OnEditToggleAllFolds)
    EVT_MENU(idEditFoldBlock,             MainFrame::OnEditFoldBlock)
    EVT_MENU(idEditUnfoldBlock,           MainFrame::OnEditUnfoldBlock)
    EVT_MENU(idEditToggleFoldBlock,       MainFrame::OnEditToggleFoldBlock)
    EVT_MENU(idEditEOLCRLF,               MainFrame::OnEditEOLMode)
    EVT_MENU(idEditEOLCR,                 MainFrame::OnEditEOLMode)
    EVT_MENU(idEditEOLLF,                 MainFrame::OnEditEOLMode)
    EVT_MENU(idEditEncodingDefault,       MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUseBom,        MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingAscii,         MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUtf7,          MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUtf8,          MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUnicode,       MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUtf16,         MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUtf32,         MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUnicode16BE,   MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUnicode16LE,   MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUnicode32BE,   MainFrame::OnEditEncoding)
    EVT_MENU(idEditEncodingUnicode32LE,   MainFrame::OnEditEncoding)
    EVT_MENU(idEditParaUp,                MainFrame::OnEditParaUp)
    EVT_MENU(idEditParaUpExtend,          MainFrame::OnEditParaUpExtend)
    EVT_MENU(idEditParaDown,              MainFrame::OnEditParaDown)
    EVT_MENU(idEditParaDownExtend,        MainFrame::OnEditParaDownExtend)
    EVT_MENU(idEditWordPartLeft,          MainFrame::OnEditWordPartLeft)
    EVT_MENU(idEditWordPartLeftExtend,    MainFrame::OnEditWordPartLeftExtend)
    EVT_MENU(idEditWordPartRight,         MainFrame::OnEditWordPartRight)
    EVT_MENU(idEditWordPartRightExtend,   MainFrame::OnEditWordPartRightExtend)
    EVT_MENU(idEditZoomIn,                MainFrame::OnEditZoomIn)
    EVT_MENU(idEditZoomOut,               MainFrame::OnEditZoomOut)
    EVT_MENU(idEditZoomReset,             MainFrame::OnEditZoomReset)
    EVT_MENU(idEditLineCut,               MainFrame::OnEditLineCut)
    EVT_MENU(idEditLineDelete,            MainFrame::OnEditLineDelete)
    EVT_MENU(idEditLineDuplicate,         MainFrame::OnEditLineDuplicate)
    EVT_MENU(idEditLineTranspose,         MainFrame::OnEditLineTranspose)
    EVT_MENU(idEditLineCopy,              MainFrame::OnEditLineCopy)
    EVT_MENU(idEditLinePaste,             MainFrame::OnEditLinePaste)
    EVT_MENU(idEditLineUp,                MainFrame::OnEditLineMove)
    EVT_MENU(idEditLineDown,              MainFrame::OnEditLineMove)
    EVT_MENU(idEditUpperCase,             MainFrame::OnEditUpperCase)
    EVT_MENU(idEditLowerCase,             MainFrame::OnEditLowerCase)
    EVT_MENU(idEditInsertNewLine,         MainFrame::OnEditInsertNewLine)
    EVT_MENU(idEditGotoLineEnd,           MainFrame::OnEditGotoLineEnd)
    EVT_MENU(idEditInsertNewLineBelow,    MainFrame::OnEditInsertNewLineBelow)
    EVT_MENU(idEditInsertNewLineAbove,    MainFrame::OnEditInsertNewLineAbove)
    EVT_MENU(idEditSelectAll,             MainFrame::OnEditSelectAll)
    EVT_MENU(idEditSelectNext,            MainFrame::OnEditSelectNext)
    EVT_MENU(idEditSelectNextSkip,        MainFrame::OnEditSelectNextSkip)
    EVT_MENU(idEditBookmarksToggle,       MainFrame::OnEditBookmarksToggle)
    EVT_MENU(idEditBookmarksNext,         MainFrame::OnEditBookmarksNext)
    EVT_MENU(idEditBookmarksPrevious,     MainFrame::OnEditBookmarksPrevious)
    EVT_MENU(idEditBookmarksClearAll,     MainFrame::OnEditBookmarksClearAll)
    EVT_MENU(idEditCommentSelected,       MainFrame::OnEditCommentSelected)
    EVT_MENU(idEditUncommentSelected,     MainFrame::OnEditUncommentSelected)
    EVT_MENU(idEditToggleCommentSelected, MainFrame::OnEditToggleCommentSelected)
    EVT_MENU(idEditStreamCommentSelected, MainFrame::OnEditStreamCommentSelected)
    EVT_MENU(idEditBoxCommentSelected,    MainFrame::OnEditBoxCommentSelected)
    EVT_MENU(idEditShowCallTip,           MainFrame::OnEditShowCallTip)
    EVT_MENU(idEditCompleteCode,          MainFrame::OnEditCompleteCode)

    EVT_MENU(idSearchFind,                  MainFrame::OnSearchFind)
    EVT_MENU(idSearchFindInFiles,           MainFrame::OnSearchFind)
    EVT_MENU(idSearchFindNext,              MainFrame::OnSearchFindNext)
    EVT_MENU(idSearchFindPrevious,          MainFrame::OnSearchFindNext)
    EVT_MENU(idSearchFindSelectedNext,      MainFrame::OnSearchFindNextSelected)
    EVT_MENU(idSearchFindSelectedPrevious,  MainFrame::OnSearchFindNextSelected)
    EVT_MENU(idSearchReplace,               MainFrame::OnSearchReplace)
    EVT_MENU(idSearchReplaceInFiles,        MainFrame::OnSearchReplace)
    EVT_MENU(idSearchGotoLine,              MainFrame::OnSearchGotoLine)
    EVT_MENU(idSearchGotoNextChanged,       MainFrame::OnSearchGotoNextChanged)
    EVT_MENU(idSearchGotoPreviousChanged,   MainFrame::OnSearchGotoPrevChanged)

    EVT_MENU(idViewLayoutSave,            MainFrame::OnViewLayoutSave)
    EVT_MENU(idViewLayoutDelete,          MainFrame::OnViewLayoutDelete)
    EVT_MENU(idViewToolFit,               MainFrame::OnViewToolbarsFit)
    EVT_MENU(idViewToolOptimize,          MainFrame::OnViewToolbarsOptimize)
    EVT_MENU(idViewToolMain,              MainFrame::OnToggleBar)
    EVT_MENU(idViewToolDebugger,          MainFrame::OnToggleBar)
    EVT_MENU(idViewLogManager,            MainFrame::OnToggleBar)
    EVT_MENU(idViewManager,               MainFrame::OnToggleBar)
    EVT_MENU(idViewStatusbar,             MainFrame::OnToggleStatusBar)
    EVT_MENU(idViewScriptConsole,         MainFrame::OnViewScriptConsole)
    EVT_MENU(idViewHideEditorTabs,        MainFrame::OnViewHideEditorTabs)
    EVT_MENU(idViewFocusEditor,           MainFrame::OnFocusEditor)
    EVT_MENU(idViewFocusManagement,       MainFrame::OnFocusManagement)
    EVT_MENU(idViewFocusLogsAndOthers,    MainFrame::OnFocusLogsAndOthers)
    EVT_MENU(idViewSwitchTabs,            MainFrame::OnSwitchTabs)
    EVT_MENU(idViewFullScreen,            MainFrame::OnToggleFullScreen)
    EVT_MENU(idViewStartPage,             MainFrame::OnToggleStartPage)

    EVT_MENU(idSettingsEnvironment,    MainFrame::OnSettingsEnvironment)
    EVT_MENU(idSettingsGlobalUserVars, MainFrame::OnGlobalUserVars)
    EVT_MENU(idSettingsBackticks,      MainFrame::OnBackticks)
    EVT_MENU(idSettingsEditor,         MainFrame::OnSettingsEditor)
    EVT_MENU(idSettingsCompiler,       MainFrame::OnSettingsCompiler)
    EVT_MENU(idSettingsDebugger,       MainFrame::OnSettingsDebugger)
    EVT_MENU(idPluginsManagePlugins,   MainFrame::OnSettingsPlugins)
    EVT_MENU(idSettingsScripting,      MainFrame::OnSettingsScripting)

    EVT_MENU(wxID_ABOUT, MainFrame::OnHelpAbout)
    EVT_MENU(idHelpTips, MainFrame::OnHelpTips)

    EVT_MENU(idStartHerePageLink,     MainFrame::OnStartHereLink)

    EVT_CBAUIBOOK_LEFT_DCLICK(ID_NBEditorManager, MainFrame::OnNotebookDoubleClick)
    EVT_NOTEBOOK_PAGE_CHANGED(ID_NBEditorManager, MainFrame::OnPageChanged)

    // Highlight status bar button
    EVT_BUTTON(idHighlightButton, MainFrame::OnHighlightMenu)
    /// CloseFullScreen event handling
    EVT_BUTTON(idCloseFullScreen, MainFrame::OnToggleFullScreen)

    /// Shift-Tab bug workaround
    EVT_MENU(idShiftTab,   MainFrame::OnShiftTab)
    EVT_MENU(idCtrlAltTab, MainFrame::OnCtrlAltTab)

    // Let plugins get copy of global accelerators
    EVT_MENU(idGetGlobalAccels,       MainFrame::OnGetGlobalAccels)

    /// Used for mouse right click in the free area of MainFrame
    EVT_RIGHT_UP(MainFrame::OnMouseRightUp)

END_EVENT_TABLE()

MainFrame::MainFrame(wxWindow* parent)
       : wxFrame(parent, -1, "MainWin", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE),
       m_LayoutManager(this),
       m_pAccel(nullptr),
       m_filesHistory(_("&File"), "/recent_files", idFileOpenRecentFileClearHistory, wxID_CBFILE01),
       m_projectsHistory(_("&File"), "/recent_projects", idFileOpenRecentProjectClearHistory, wxID_CBFILE17),
       m_pCloseFullScreenBtn(nullptr),
       m_pEdMan(nullptr),
       m_pPrjMan(nullptr),
       m_pPrjManUI(nullptr),
       m_pLogMan(nullptr),
       m_pInfoPane(nullptr),
       m_pToolbar(nullptr),
       m_ToolsMenu(nullptr),
       m_HelpPluginsMenu(nullptr),
       m_ScanningForPlugins(false),
       m_StartupDone(false), // one-time flag
       m_InitiatedShutdown(false),
       m_AutoHideLockCounter(0),
       m_LastCtrlAltTabWindow(0),
       m_LastLayoutIsTemp(false),
       m_pScriptConsole(nullptr),
       m_pBatchBuildDialog(nullptr)
{
    Manager::Get(this); // provide manager with handle to MainFrame (this)

    // register event sinks
    RegisterEvents();

    // New: Allow drag and drop of files into the editor
    SetDropTarget(new cbFileDropTarget(this));

    // Accelerator table
    m_AccelCount = 8;
    m_pAccelEntries.reset(new wxAcceleratorEntry[m_AccelCount]);
    m_pAccelEntries[0].Set(wxACCEL_CTRL | wxACCEL_SHIFT,  (int) 'W', idFileCloseAll);
    m_pAccelEntries[1].Set(wxACCEL_CTRL | wxACCEL_SHIFT,  WXK_F4,    idFileCloseAll);
    m_pAccelEntries[2].Set(wxACCEL_CTRL,                  (int) 'W', idFileClose);
    m_pAccelEntries[3].Set(wxACCEL_CTRL,                  WXK_F4,    idFileClose);
    m_pAccelEntries[4].Set(wxACCEL_CTRL,                  WXK_F6,    idFileNext);
    m_pAccelEntries[5].Set(wxACCEL_CTRL | wxACCEL_SHIFT,  WXK_F6,    idFilePrev);
    m_pAccelEntries[6].Set(wxACCEL_SHIFT,                 WXK_TAB,   idShiftTab);
    m_pAccelEntries[7].Set(wxACCEL_CTRL | wxACCEL_ALT,    WXK_TAB,   idCtrlAltTab);
    m_pAccel.reset(new wxAcceleratorTable(m_AccelCount, m_pAccelEntries.get()));

    SetAcceleratorTable(*m_pAccel);

    // add file filters for supported projects/workspaces
    FileFilters::AddDefaultFileFilters();

    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");

    // We want to restore the size of the windows as early as possible, so things like
    // GetClientSize() would return proper values. F.e. if we call this after the creation of the
    // status bar it is possible that the first field in it would be calculated with zero or
    // negative width and the second field would be in incorrect place.
    LoadWindowSize();

    CreateIDE();

#ifdef __WXMSW__
    SetIcon(wxICON(A_MAIN_ICON));
#else
    SetIcon(wxIcon(app_xpm));
#endif // __WXMSW__

    // Even it is possible that the statusbar is not visible at the moment,
    // create the statusbar so the plugins can create their own fields on the it
    wxStatusBar *sb = CreateStatusBar(MainStatusBar::numFields);
    if (sb)
        sb->Show(cfg->ReadBool("/main_frame/statusbar", true));

    SetTitle(appglobals::AppName + " v" + appglobals::AppVersion);

    ScanForPlugins();
    if (!Manager::IsBatchBuild())
        CreateToolbars();

    Manager::Get()->GetCCManager();

    // save default view
    const wxString deflayout(cfg->Read("/main_frame/layout/default"));
    if (deflayout.empty())
        cfg->Write("/main_frame/layout/default", gDefaultLayout);

    gDefaultLayoutData = m_LayoutManager.SavePerspective(); // keep the "hardcoded" layout handy
    gDefaultMessagePaneLayoutData = m_pInfoPane->SaveTabOrder();
    SaveViewLayout(gDefaultLayout, gDefaultLayoutData, gDefaultMessagePaneLayoutData);

    // generate default minimal layout
    wxAuiPaneInfoArray& panes = m_LayoutManager.GetAllPanes();
    for (size_t i = 0; i < panes.GetCount(); ++i)
    {
        wxAuiPaneInfo& info = panes[i];
        if (info.name != "MainPane")
            info.Hide();
    }
    gMinimalLayoutData = m_LayoutManager.SavePerspective(); // keep the "hardcoded" layout handy
    gMinimalMessagePaneLayoutData = m_pInfoPane->SaveTabOrder();
    SaveViewLayout(gMinimalLayout, gMinimalLayoutData, gMinimalMessagePaneLayoutData);

    LoadWindowState();

    ShowHideStartPage();

    RegisterScriptFunctions();
    RunStartupScripts();

    Manager::Get()->GetLogManager()->DebugLog(_("Initializing plugins..."));
}

MainFrame::~MainFrame()
{
    SetAcceleratorTable(wxNullAcceleratorTable);

    DeInitPrinting();

    delete m_debuggerMenuHandler;
    delete m_debuggerToolbarHandler;
}

void MainFrame::RegisterEvents()
{
    Manager* m = Manager::Get();

    m->RegisterEventSink(cbEVT_EDITOR_UPDATE_UI,       new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorUpdateUI));

    m->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,       new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnProjectActivated));
    m->RegisterEventSink(cbEVT_PROJECT_OPEN,           new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnProjectOpened));
    m->RegisterEventSink(cbEVT_PROJECT_CLOSE,          new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnProjectClosed));
    m->RegisterEventSink(cbEVT_EDITOR_CLOSE,           new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorClosed));
    m->RegisterEventSink(cbEVT_EDITOR_OPEN,            new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorOpened));
    m->RegisterEventSink(cbEVT_EDITOR_ACTIVATED,       new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorActivated));
    m->RegisterEventSink(cbEVT_EDITOR_SAVE,            new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorSaved));
    m->RegisterEventSink(cbEVT_EDITOR_MODIFIED,        new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnEditorModified));

    m->RegisterEventSink(cbEVT_ADD_DOCK_WINDOW,        new cbEventFunctor<MainFrame, CodeBlocksDockEvent>(this, &MainFrame::OnRequestDockWindow));
    m->RegisterEventSink(cbEVT_REMOVE_DOCK_WINDOW,     new cbEventFunctor<MainFrame, CodeBlocksDockEvent>(this, &MainFrame::OnRequestUndockWindow));
    m->RegisterEventSink(cbEVT_SHOW_DOCK_WINDOW,       new cbEventFunctor<MainFrame, CodeBlocksDockEvent>(this, &MainFrame::OnRequestShowDockWindow));
    m->RegisterEventSink(cbEVT_HIDE_DOCK_WINDOW,       new cbEventFunctor<MainFrame, CodeBlocksDockEvent>(this, &MainFrame::OnRequestHideDockWindow));
    m->RegisterEventSink(cbEVT_DOCK_WINDOW_VISIBILITY, new cbEventFunctor<MainFrame, CodeBlocksDockEvent>(this, &MainFrame::OnDockWindowVisibility));

    m->RegisterEventSink(cbEVT_PLUGIN_ATTACHED,        new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnPluginLoaded));
    m->RegisterEventSink(cbEVT_PLUGIN_RELEASED,        new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnPluginUnloaded));
    m->RegisterEventSink(cbEVT_PLUGIN_INSTALLED,       new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnPluginInstalled));
    m->RegisterEventSink(cbEVT_PLUGIN_UNINSTALLED,     new cbEventFunctor<MainFrame, CodeBlocksEvent>(this, &MainFrame::OnPluginUninstalled));

    m->RegisterEventSink(cbEVT_UPDATE_VIEW_LAYOUT,     new cbEventFunctor<MainFrame, CodeBlocksLayoutEvent>(this, &MainFrame::OnLayoutUpdate));
    m->RegisterEventSink(cbEVT_QUERY_VIEW_LAYOUT,      new cbEventFunctor<MainFrame, CodeBlocksLayoutEvent>(this, &MainFrame::OnLayoutQuery));
    m->RegisterEventSink(cbEVT_SWITCH_VIEW_LAYOUT,     new cbEventFunctor<MainFrame, CodeBlocksLayoutEvent>(this, &MainFrame::OnLayoutSwitch));

    m->RegisterEventSink(cbEVT_ADD_LOG_WINDOW,         new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnAddLogWindow));
    m->RegisterEventSink(cbEVT_REMOVE_LOG_WINDOW,      new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnRemoveLogWindow));
    m->RegisterEventSink(cbEVT_HIDE_LOG_WINDOW,        new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnHideLogWindow));
    m->RegisterEventSink(cbEVT_SWITCH_TO_LOG_WINDOW,   new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnSwitchToLogWindow));
    m->RegisterEventSink(cbEVT_GET_ACTIVE_LOG_WINDOW,  new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnGetActiveLogWindow));
    m->RegisterEventSink(cbEVT_SHOW_LOG_MANAGER,       new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnShowLogManager));
    m->RegisterEventSink(cbEVT_HIDE_LOG_MANAGER,       new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnHideLogManager));
    m->RegisterEventSink(cbEVT_LOCK_LOG_MANAGER,       new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnLockLogManager));
    m->RegisterEventSink(cbEVT_UNLOCK_LOG_MANAGER,     new cbEventFunctor<MainFrame, CodeBlocksLogEvent>(this, &MainFrame::OnUnlockLogManager));
}

void MainFrame::ShowTips(bool forceShow)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    bool showAtStartup = cfg->ReadBool("/show_tips", false);
    if (forceShow || showAtStartup)
    {
        wxString tipsFile = ConfigManager::GetDataFolder() + "/tips.txt";
        long tipsIndex = cfg->ReadInt("/next_tip", 0);
        wxTipProvider* tipProvider = wxCreateFileTipProvider(tipsFile, tipsIndex);
        showAtStartup = wxShowTip(this, tipProvider, showAtStartup);
        cfg->Write("/show_tips", showAtStartup);
        cfg->Write("/next_tip", (int)tipProvider->GetCurrentTip());
        delete tipProvider;
    }
}

void MainFrame::CreateIDE()
{
    int leftW = Manager::Get()->GetConfigManager("app")->ReadInt("/main_frame/layout/left_block_width", 200);
    wxSize clientsize = GetClientSize();

    // Create CloseFullScreen Button, and hide it initially
    m_pCloseFullScreenBtn = new wxButton(this, idCloseFullScreen, _( "Close full screen" ), wxDefaultPosition );
    m_pCloseFullScreenBtn->Show( false );

    // management panel
    m_pPrjMan = Manager::Get()->GetProjectManager();
    if (!Manager::IsBatchBuild())
    {
        m_pPrjManUI = new ProjectManagerUI;
        m_LayoutManager.AddPane( m_pPrjManUI->GetNotebook(),
                                 wxAuiPaneInfo().Name("ManagementPane").Caption(_("Management")).
                                     BestSize(wxSize(leftW, clientsize.GetHeight())).
                                     MinSize(wxSize(100,100)).Left().Layer(1) );
    }
    else
        m_pPrjManUI = new BatchProjectManagerUI;

    m_pPrjMan->SetUI(m_pPrjManUI);

    const double scaleFactor = cbGetContentScaleFactor(*this);
    const int targetHeight = wxRound(16 * scaleFactor);
    const int uiSize16 = cbFindMinSize16to64(targetHeight);

    // All message posted before this call are either lost or sent to stdout/stderr.
    // On windows stdout and stderr aren't accessible.
    SetupGUILogging(uiSize16);

    {
        wxString msg = wxString::Format(_("Loaded config file '%s' (personality: '%s')"),
                                        CfgMgrBldr::Get()->GetConfigFile(),
                                        Manager::Get()->GetPersonalityManager()->GetPersonality());
        Manager::Get()->GetLogManager()->Log(msg);
    }

    SetupDebuggerUI();

    {
        // Setup the art provider with the images stored in manager_resources.zip
        const wxString prefix(ConfigManager::GetDataFolder()+ "/manager_resources.zip#zip:/images");
        cbArtProvider* provider = new cbArtProvider(prefix);

#if wxCHECK_VERSION(3, 1, 6)
        const wxString ext(".svg");
#else
        const wxString ext(".png");
#endif

        provider->AddMapping("sdk/select_target", "select_target"+ext);
        provider->AddMapping("sdk/missing_icon",  "missing_icon"+ext);

        wxArtProvider::Push(provider);
    }

    {
        // Setup the art provider for the main menu. Use scaling factor detection to determine the
        // size of the images. Also do this here when we have a main window (probably this doesn't
        // help us much, because the window hasn't been shown yet).

        // Setup menu sizes
        Manager::Get()->SetImageSize(uiSize16, Manager::UIComponent::Menus);
        Manager::Get()->SetUIScaleFactor(scaleFactor, Manager::UIComponent::Menus);

        // Setup main sizes
        Manager::Get()->SetImageSize(uiSize16, Manager::UIComponent::Main);
        Manager::Get()->SetUIScaleFactor(scaleFactor, Manager::UIComponent::Main);

        // Setup toolbar sizes
        const int configSize = cbHelpers::ReadToolbarSizeFromConfig();
        const int scaledSize = cbFindMinSize16to64(wxRound(configSize * scaleFactor));
        Manager::Get()->SetImageSize(scaledSize, Manager::UIComponent::Toolbars);
        Manager::Get()->SetUIScaleFactor(scaleFactor, Manager::UIComponent::Toolbars);

        const wxString prefix(ConfigManager::GetDataFolder() + "/resources.zip#zip:/images");
        cbArtProvider* provider = new cbArtProvider(prefix);

#if wxCHECK_VERSION(3, 1, 6)
        const wxString ext(".svg");
#else
        const wxString ext(".png");
#endif

        provider->AddMapping("core/file_open", "fileopen"+ext);
        provider->AddMapping("core/file_new", "filenew"+ext);
        provider->AddMapping("core/history_clear", "history_clear"+ext);
        provider->AddMapping("core/file_save", "filesave"+ext);
        provider->AddMapping("core/file_save_as", "filesaveas"+ext);
        provider->AddMapping("core/file_save_all", "filesaveall"+ext);
        provider->AddMapping("core/file_close", "fileclose"+ext);
        provider->AddMapping("core/file_print", "fileprint"+ext);
        provider->AddMapping("core/exit", "exit"+ext);
        provider->AddMapping("core/undo", "undo"+ext);
        provider->AddMapping("core/redo", "redo"+ext);
        provider->AddMapping("core/edit_cut", "editcut"+ext);
        provider->AddMapping("core/edit_copy", "editcopy"+ext);
        provider->AddMapping("core/edit_paste", "editpaste"+ext);
        provider->AddMapping("core/bookmark_add", "bookmark_add"+ext);
        provider->AddMapping("core/find", "filefind"+ext);
        provider->AddMapping("core/find_in_files", "findf"+ext);
        provider->AddMapping("core/find_next", "filefindnext"+ext);
        provider->AddMapping("core/find_prev", "filefindprev"+ext);
        provider->AddMapping("core/search_replace", "searchreplace"+ext);
        provider->AddMapping("core/search_replace_in_files", "searchreplacef"+ext);
        provider->AddMapping("core/goto", "goto"+ext);
        provider->AddMapping("core/manage_plugins", "plug"+ext);
        provider->AddMapping("core/help_info", "info"+ext);
        provider->AddMapping("core/help_idea", "idea"+ext);

        provider->AddMapping("core/dbg/run", "dbgrun"+ext);
        provider->AddMapping("core/dbg/pause", "dbgpause"+ext);
        provider->AddMapping("core/dbg/stop", "dbgstop"+ext);
        provider->AddMapping("core/dbg/run_to", "dbgrunto"+ext);
        provider->AddMapping("core/dbg/next", "dbgnext"+ext);
        provider->AddMapping("core/dbg/step", "dbgstep"+ext);
        provider->AddMapping("core/dbg/step_out", "dbgstepout"+ext);
        provider->AddMapping("core/dbg/next_inst", "dbgnexti"+ext);
        provider->AddMapping("core/dbg/step_inst", "dbgstepi"+ext);
        provider->AddMapping("core/dbg/window", "dbgwindow"+ext);
        provider->AddMapping("core/dbg/info", "dbginfo"+ext);

        provider->AddMappingF("core/folder_open", "tree/%dx%d/folder_open"+ext);
        provider->AddMappingF("core/gear", "infopane/%dx%d/misc"+ext);

        wxArtProvider::Push(provider);
    }

    CreateMenubar();

    m_pEdMan  = Manager::Get()->GetEditorManager();
    m_pLogMan = Manager::Get()->GetLogManager();

    // editor manager
    m_LayoutManager.AddPane(m_pEdMan->GetNotebook(), wxAuiPaneInfo().Name("MainPane").
                            CentrePane());

    // script console
    m_pScriptConsole = new ScriptConsole(this, -1);
    m_LayoutManager.AddPane(m_pScriptConsole, wxAuiPaneInfo().Name("ScriptConsole").
                            Caption(_("Scripting console")).Float().MinSize(100,100).FloatingPosition(300, 200).Hide());

    DoUpdateLayout();
    DoUpdateLayoutColours();
    DoUpdateEditorStyle();

    m_pEdMan->GetNotebook()->SetDropTarget(new cbFileDropTarget(this));

    Manager::Get()->GetColourManager()->Load();
}


void MainFrame::SetupGUILogging(int uiSize16)
{
    // allow new docked windows to use be 3/4 of the available space, the default (0.3) is sometimes too small, especially for "Logs & others"
    m_LayoutManager.SetDockSizeConstraint(0.75,0.75);

    int bottomH = Manager::Get()->GetConfigManager("app")->ReadInt("/main_frame/layout/bottom_block_height", 150);
    wxSize clientsize = GetClientSize();

    LogManager* mgr = Manager::Get()->GetLogManager();
    Manager::Get()->SetImageSize(uiSize16, Manager::UIComponent::InfoPaneNotebooks);
    Manager::Get()->SetUIScaleFactor(cbGetContentScaleFactor(*this),
                                     Manager::UIComponent::InfoPaneNotebooks);

    if (!Manager::IsBatchBuild())
    {
        m_pInfoPane = new InfoPane(this);
        m_LayoutManager.AddPane(m_pInfoPane, wxAuiPaneInfo().
                                  Name("MessagesPane").Caption(_("Logs & others")).
                                  BestSize(wxSize(clientsize.GetWidth(), bottomH)).//MinSize(wxSize(50,50)).
                                  Bottom());

        wxWindow* log;

        for (size_t i = LogManager::app_log; i < LogManager::max_logs; ++i)
        {
            if ((log = mgr->Slot(i).GetLogger()->CreateControl(m_pInfoPane)))
                m_pInfoPane->AddLogger(mgr->Slot(i).GetLogger(), log, mgr->Slot(i).title, mgr->Slot(i).icon);
        }

        m_findReplace.CreateSearchLog();
    }
    else
    {
        m_pBatchBuildDialog = new BatchLogWindow(this, _("Code::Blocks - Batch build"));
        wxSizer* s = new wxBoxSizer(wxVERTICAL);
        m_pInfoPane = new InfoPane(m_pBatchBuildDialog);
        s->Add(m_pInfoPane, 1, wxEXPAND);
        m_pBatchBuildDialog->SetSizer(s);
    }

    mgr->NotifyUpdate();
    m_pInfoPane->SetDropTarget(new cbFileDropTarget(this));
}

void MainFrame::SetupDebuggerUI()
{
    m_debuggerMenuHandler = new DebuggerMenuHandler;
    m_debuggerToolbarHandler = new DebuggerToolbarHandler(m_debuggerMenuHandler);
    m_debuggerMenuHandler->SetEvtHandlerEnabled(false);
    m_debuggerToolbarHandler->SetEvtHandlerEnabled(false);
    wxWindow* window = Manager::Get()->GetAppWindow();
    if (window)
    {
        window->PushEventHandler(m_debuggerMenuHandler);
        window->PushEventHandler(m_debuggerToolbarHandler);
    }
    m_debuggerMenuHandler->SetEvtHandlerEnabled(true);
    m_debuggerToolbarHandler->SetEvtHandlerEnabled(true);

    if (!Manager::IsBatchBuild())
    {
        Manager::Get()->GetDebuggerManager()->SetInterfaceFactory(new DebugInterfaceFactory);
        m_debuggerMenuHandler->RegisterDefaultWindowItems();
    }
}

SQInteger MainFrame_Open(HSQUIRRELVM v)
{
    MainFrame *mainFrame = static_cast<MainFrame*>(Manager::Get()->GetAppFrame());
    if (!mainFrame)
        return sq_throwerror(v, _SC("MainFrame::Open: No access to the MainFrame object!"));

    using namespace ScriptBindings;
    // env table, filename, addToHistory
    ExtractParams3<SkipParam, const wxString*, bool> extractor(v);
    if (!extractor.Process("MainFrame::Open"))
        return extractor.ErrorMessage();

    sq_pushbool(v, mainFrame->Open(*extractor.p1, extractor.p2));
    return 1;
}

/// Register a squirrel table 'App' which has a function 'Open' which calls MainFrame::Open.
/// Used for showing html help files.
void MainFrame::RegisterScriptFunctions()
{
    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    HSQUIRRELVM v = scriptMgr->GetVM();

    using namespace ScriptBindings;

    PreserveTop preserveTop(v);
    sq_pushroottable(v);

    sq_pushstring(v, _SC("App"), -1);
    sq_newtable(v);
    BindStaticMethod(v, _SC("Open"), MainFrame_Open, _SC("MainFrame::Open"));

    sq_newslot(v, -3, false); // Add the 'App' table to the root table
    sq_poptop(v); // Pop root table
}

void MainFrame::RunStartupScripts()
{
    ConfigManager* mgr = Manager::Get()->GetConfigManager("scripting");
    wxArrayString keys = mgr->EnumerateKeys("/startup_scripts");

    for (size_t i = 0; i < keys.GetCount(); ++i)
    {
        ScriptEntry se;
        wxString ser;
        if (mgr->Read("/startup_scripts/" + keys[i], &ser))
        {
            se.SerializeIn(ser);
            if (!se.enabled)
                continue;

            wxString startup(se.script);
            if (wxFileName(se.script).IsRelative())
                startup = ConfigManager::LocateDataFile(se.script, sdScriptsUser | sdScriptsGlobal);

            if (!startup.empty())
            {
                if (!se.registered)
                    Manager::Get()->GetScriptingManager()->LoadScript(startup);
                else if (!se.menu.empty())
                    Manager::Get()->GetScriptingManager()->RegisterScriptMenu(se.menu, startup, false);
                else
                    Manager::Get()->GetLogManager()->LogWarning(wxString::Format(_("Startup script/function '%s' not loaded: invalid configuration"), se.script));
            }
            else
                Manager::Get()->GetLogManager()->LogWarning(wxString::Format(_("Startup script '%s' not found"), se.script));
        }
    }
}

void MainFrame::PluginsUpdated(cb_unused cbPlugin* plugin, cb_unused int status)
{
    Freeze();

    // menu
    RecreateMenuBar();

    // update view->toolbars because we re-created the menubar
    PluginElementsArray plugins = Manager::Get()->GetPluginManager()->GetPlugins();
    const size_t pluginCount = plugins.GetCount();
    for (size_t i = 0; i < pluginCount; ++i)
    {
        cbPlugin* plug = plugins[i]->plugin;
        const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(plug);
        if (!info)
            continue;

        if (m_PluginsTools[plug]) // if plugin has a toolbar
        {
            // toolbar exists; add the menu item
            wxMenu* viewToolbars = nullptr;
            GetMenuBar()->FindItem(idViewToolMain, &viewToolbars);
            if (viewToolbars)
            {
                if (viewToolbars->FindItem(info->title) != wxNOT_FOUND)
                    continue;

                wxMenuItem* item = AddPluginInMenus(viewToolbars, plug,
                                                    (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&MainFrame::OnToggleBar,
                                                    -1, true);
                if (item)
                {
                    item->Check(IsWindowReallyShown(m_PluginsTools[plug]));
                }
            }
        }
    }

    Thaw();
}

void MainFrame::RecreateMenuBar()
{
    Freeze();

    wxMenuBar* m = GetMenuBar();
    SetMenuBar(nullptr); // unhook old menubar
    CreateMenubar(); // create new menubar
    delete m; // delete old menubar

    // update layouts menu
    for (LayoutViewsMap::iterator it = m_LayoutViews.begin(); it != m_LayoutViews.end(); ++it)
    {
        if (it->first.empty())
            continue;
        SaveViewLayout(it->first, it->second,
                       m_LayoutMessagePane[it->first],
                       it->first == m_LastLayoutName);
    }

    Thaw();
}

void MainFrame::CreateMenubar()
{
    CodeBlocksEvent event(cbEVT_MENUBAR_CREATE_BEGIN);
    Manager::Get()->ProcessEvent(event);

    int tmpidx;
    wxMenuBar* mbar = nullptr;
    wxMenu *tools = nullptr, *plugs = nullptr, *pluginsM = nullptr;
    wxMenuItem *tmpitem = nullptr;

    wxXmlResource* xml_res = wxXmlResource::Get();
    xml_res->Load(ConfigManager::GetDataFolder() + "/resources.zip#zip:main_menu.xrc");
    Manager::Get()->GetLogManager()->DebugLog("Loading menubar...");
    mbar = xml_res->LoadMenuBar("main_menu_bar");
    if (!mbar)
        mbar = new wxMenuBar(); // Some error happened.
    if (mbar)
        SetMenuBar(mbar);

    // Find Menus that we'll change later

    tmpidx = mbar->FindMenu(_("&Edit"));
    if (tmpidx!=wxNOT_FOUND)
    {
        wxMenu *hl = nullptr;
        mbar->FindItem(idEditHighlightModeText, &hl);
        EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();

        if (hl && colour_set)
        {
            wxArrayString langs = colour_set->GetAllHighlightLanguages();
            for (size_t i = 0; i < langs.GetCount(); ++i)
            {
                if (i > 0 && !(i % 20))
                    hl->Break(); // break into columns every 20 items

                const wxString &lang = langs[i];
                bool found = false;
                int id = -1;
                for (const MenuIDToLanguage::value_type &menuIDToLanguage : m_MapMenuIDToLanguage)
                {
                    if (menuIDToLanguage.second == lang)
                    {
                        found = true;
                        id = menuIDToLanguage.first;
                        break;
                    }
                }

                if (!found)
                {
                    id = wxNewId();
                    m_MapMenuIDToLanguage.insert(MenuIDToLanguage::value_type(id, lang));
                }

                hl->AppendRadioItem(id, lang,
                                    wxString::Format(_("Switch highlighting mode for current document to \"%s\""),
                                                     lang));
                Connect(id, wxEVT_COMMAND_MENU_SELECTED,
                        wxObjectEventFunction(&MainFrame::OnEditHighlightMode));
                Connect(id, wxEVT_UPDATE_UI,
                        wxObjectEventFunction(&MainFrame::OnEditHighlightModeUpdateUI));
            }
        }
        const wxLanguageInfo* info = wxLocale::GetLanguageInfo(wxLANGUAGE_DEFAULT);
        wxMenu* editMenu = mbar->GetMenu(tmpidx);
        if (   info
            && ( ( info->Language >= wxLANGUAGE_CHINESE
                  && info->Language <= wxLANGUAGE_CHINESE_TAIWAN )
                || info->Language == wxLANGUAGE_JAPANESE
                || info->Language == wxLANGUAGE_KOREAN ) )
        {
            editMenu->Append(idEditCompleteCode, _("Complete code\tShift-Space"));
        }
        else
            editMenu->Append(idEditCompleteCode, _("Complete code\tCtrl-Space"));
    }

    tmpidx = mbar->FindMenu(_("&Tools"));
    if (tmpidx!=wxNOT_FOUND)
        tools = mbar->GetMenu(tmpidx);

    tmpidx = mbar->FindMenu(_("P&lugins"));
    if (tmpidx!=wxNOT_FOUND)
        plugs = mbar->GetMenu(tmpidx);

    if ((tmpitem = mbar->FindItem(idHelpPlugins,nullptr)))
        pluginsM = tmpitem->GetSubMenu();

    m_ToolsMenu       = tools    ? tools    : new wxMenu();
    m_PluginsMenu     = plugs    ? plugs    : new wxMenu();
    m_HelpPluginsMenu = pluginsM ? pluginsM : new wxMenu();

    // core modules: create menus
    if (!Manager::IsBatchBuild())
        static_cast<ProjectManagerUI*>(m_pPrjManUI)->CreateMenu(mbar);
    Manager::Get()->GetDebuggerManager()->SetMenuHandler(m_debuggerMenuHandler);

    // ask all plugins to rebuild their menus
    PluginElementsArray plugins = Manager::Get()->GetPluginManager()->GetPlugins();
    for (unsigned int i = 0; i < plugins.GetCount(); ++i)
    {
        cbPlugin* plug = plugins[i]->plugin;
        if (plug && plug->IsAttached())
        {
            if (plug->GetType() == ptTool)
                DoAddPlugin(plug);
            else
            {
                AddPluginInHelpPluginsMenu(plug);
                try
                {
                    plug->BuildMenu(mbar);
                }
                catch (cbException& e)
                {
                    e.ShowErrorMessage();
                }
            }
        }
    }

    Manager::Get()->GetToolsManager()->BuildToolsMenu(m_ToolsMenu);

    // Ctrl+Tab workaround for non windows platforms:
    if ((platform::carbon) || (platform::gtk))
    {
        // Find the menu item for tab switching:
        tmpidx = mbar->FindMenu(_("&View"));
        if (tmpidx != wxNOT_FOUND)
        {
            wxMenu* view = mbar->GetMenu(tmpidx);
            wxMenuItem* switch_item = view->FindItem(idViewSwitchTabs);
            if (switch_item)
            {
                // Change the accelerator for this menu item:
                wxString accel;
                if      (platform::carbon)
                    accel = "Alt+Tab";
                else if (platform::gtk)
                    accel = "Ctrl+,";
                switch_item->SetItemLabel(_("S&witch tabs") + wxString('\t') + accel);
            }
        }
    }

    SetMenuBar(mbar);
    InitializeRecentFilesHistory();

    CodeBlocksEvent event2(cbEVT_MENUBAR_CREATE_END);
    Manager::Get()->ProcessEvent(event2);
}

void MainFrame::CreateToolbars()
{
    if (m_pToolbar)
    {
        SetToolBar(nullptr);
        m_pToolbar = nullptr;
    }

    wxXmlResource* xml_res = wxXmlResource::Get();
    xml_res->Load(ConfigManager::GetDataFolder() + "/resources.zip#zip:main_toolbar.xrc");
    Manager::Get()->GetLogManager()->DebugLog("Loading toolbar...");

    m_pToolbar = Manager::Get()->CreateEmptyToolbar();
    Manager::Get()->AddonToolBar(m_pToolbar, "main_toolbar");

    m_pToolbar->Realize();

    // Right click on the main toolbar will popup a context menu
    m_pToolbar->Connect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED, wxCommandEventHandler(MainFrame::OnToolBarRightClick), nullptr, this);

    m_pToolbar->SetInitialSize();

    // Right click on the debugger toolbar will popup a context menu
    m_debuggerToolbarHandler->GetToolbar()->Connect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED, wxCommandEventHandler(MainFrame::OnToolBarRightClick), nullptr, this );

    std::vector<ToolbarInfo> toolbars;

    toolbars.push_back(ToolbarInfo(m_pToolbar, wxAuiPaneInfo().Name("MainToolbar").Caption(_("Main Toolbar")), 0));
    toolbars.push_back(ToolbarInfo(m_debuggerToolbarHandler->GetToolbar(),
                                   wxAuiPaneInfo(). Name("DebuggerToolbar").Caption(_("Debugger Toolbar")),
                                   2));

    // ask all plugins to rebuild their toolbars
    PluginElementsArray plugins = Manager::Get()->GetPluginManager()->GetPlugins();
    for (unsigned int i = 0; i < plugins.GetCount(); ++i)
    {
        cbPlugin* plug = plugins[i]->plugin;
        if (plug && plug->IsAttached())
        {
            ToolbarInfo info = DoAddPluginToolbar(plug);
            if (info.toolbar)
            {
                toolbars.push_back(info);
                // support showing context menu of the plugins' toolbar
                info.toolbar->Connect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED,
                                      wxCommandEventHandler(MainFrame::OnToolBarRightClick), nullptr, this );
            }
        }
    }

    std::sort(toolbars.begin(), toolbars.end());

    int row = 0, position = 0, rowLength = 0;
    int maxLength = GetSize().x;

    for (std::vector<ToolbarInfo>::iterator it = toolbars.begin(); it != toolbars.end(); ++it)
    {
        rowLength += it->toolbar->GetSize().x;
        if (rowLength >= maxLength)
        {
            row++;
            position = 0;
            rowLength = it->toolbar->GetSize().x;
        }
        wxAuiPaneInfo paneInfo(it->paneInfo);
        m_LayoutManager.AddPane(it->toolbar, paneInfo.ToolbarPane().Top().Row(row).Position(position));

        position += it->toolbar->GetSize().x;
    }
    DoUpdateLayout();

    Manager::ProcessPendingEvents();
    SetToolBar(nullptr);
}

void MainFrame::AddToolbarItem(int id, const wxString& title, const wxString& shortHelp, const wxString& longHelp, const wxString& image)
{
    m_pToolbar->AddTool(id, title, cbLoadBitmap(image, wxBITMAP_TYPE_PNG));
    m_pToolbar->SetToolShortHelp(id, shortHelp);
    m_pToolbar->SetToolLongHelp(id, longHelp);
}

void MainFrame::ScanForPlugins()
{
    m_ScanningForPlugins = true;
    m_PluginIDsMap.clear();

    PluginManager* m_PluginManager = Manager::Get()->GetPluginManager();

    // user paths first
    wxString path = ConfigManager::GetPluginsFolder(false);
    Manager::Get()->GetLogManager()->Log(wxString::Format(_("Scanning for plugins in %s"), path));
    int count = m_PluginManager->ScanForPlugins(path);

    // global paths
    path = ConfigManager::GetPluginsFolder(true);
    Manager::Get()->GetLogManager()->Log(wxString::Format(_("Scanning for plugins in %s"), path));
    count += m_PluginManager->ScanForPlugins(path);

    // actually load plugins
    if (count > 0)
    {
        Manager::Get()->GetLogManager()->Log(_("Loading:"));
        m_PluginManager->LoadAllPlugins();
    }

    CodeBlocksEvent event(cbEVT_PLUGIN_LOADING_COMPLETE);
    Manager::Get()->GetPluginManager()->NotifyPlugins(event);
    m_ScanningForPlugins = false;
}

wxMenuItem* MainFrame::AddPluginInMenus(wxMenu* menu, cbPlugin* plugin, wxObjectEventFunction callback, int pos, bool checkable)
{
    wxMenuItem* item = nullptr;
    if (!plugin || !menu)
        return item;

    const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(plugin);
    if (!info)
        return nullptr;

    PluginIDsMap::iterator it;
    for (it = m_PluginIDsMap.begin(); it != m_PluginIDsMap.end(); ++it)
    {
        if (it->second == info->name)
        {
            item = menu->FindItem(it->first);
            if (item)
                return item;
        }
    }

    int id = wxNewId();
    wxString title(info->title);
    if (menu == m_HelpPluginsMenu)
        title << "...";

    m_PluginIDsMap[id] = info->name;
    if (pos == -1)
        pos = menu->GetMenuItemCount();

    while(!item)
    {
        if (!pos || title.CmpNoCase(menu->FindItemByPosition(pos - 1)->GetItemLabelText()) > 0)
            item = menu->Insert(pos, id, title, wxEmptyString, checkable ? wxITEM_CHECK : wxITEM_NORMAL);

        --pos;
    }

    Connect(id, wxEVT_COMMAND_MENU_SELECTED, callback);
    if (checkable)
    {
        Connect(id, wxEVT_UPDATE_UI,
                wxObjectEventFunction(&MainFrame::OnUpdateCheckablePluginMenu));
    }
    return item;
}

void MainFrame::AddPluginInPluginsMenu(cbPlugin* plugin)
{
    // "Plugins" menu is special case because it contains "Manage plugins",
    // which must stay at the end of the menu
    // So we insert entries, not append...

    // this will insert a separator when the first plugin is added in the "Plugins" menu
    if (m_PluginsMenu->GetMenuItemCount() == 1)
         m_PluginsMenu->Insert(0, wxID_SEPARATOR, "");

    AddPluginInMenus(m_PluginsMenu, plugin,
                    (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&MainFrame::OnPluginsExecuteMenu,
                    m_PluginsMenu->GetMenuItemCount() - 2);
}

void MainFrame::AddPluginInHelpPluginsMenu(cbPlugin* plugin)
{
    AddPluginInMenus(m_HelpPluginsMenu, plugin,
                    (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&MainFrame::OnHelpPluginMenu);
}


namespace
{
struct ToolbarFitInfo
{
    int row;
    wxRect rect;
    wxWindow *window;

    bool operator<(const ToolbarFitInfo &r) const
    {
        if (row < r.row)
            return true;
        else if (row == r.row)
            return rect.x < r.rect.x;
        else
            return false;
    }
};

static void CollectToolbars(std::set<ToolbarFitInfo> &result, wxAuiManager &layoutManager)
{
    const wxAuiPaneInfoArray &panes = layoutManager.GetAllPanes();
    for (size_t ii = 0; ii < panes.GetCount(); ++ii)
    {
        const wxAuiPaneInfo &info = panes[ii];
        if (info.IsToolbar() && info.IsShown())
        {
            ToolbarFitInfo f;
            f.row = info.dock_row;
            f.rect = info.rect;
            f.window = info.window;
            result.insert(f);
        }
    }
}

struct ToolbarRowInfo
{
    ToolbarRowInfo() {}
    ToolbarRowInfo(int width_, int position_) : width(width_), position(position_) {}

    int width, position;
};

// Function which tries to make all toolbars visible.
static void FitToolbars(wxAuiManager &layoutManager, wxWindow *mainFrame)
{
    std::set<ToolbarFitInfo> sorted;
    CollectToolbars(sorted, layoutManager);
    if (sorted.empty())
        return;

    int maxWidth = mainFrame->GetSize().x;
    int gripperSize =  layoutManager.GetArtProvider()->GetMetric(wxAUI_DOCKART_GRIPPER_SIZE);

    // move all toolbars to the left as possible and add the non-fitting to a list
    std::vector<ToolbarRowInfo> rows;
    std::vector<wxWindow*> nonFitingToolbars;
    for (std::set<ToolbarFitInfo>::const_iterator it = sorted.begin(); it != sorted.end(); ++it)
    {
        wxAuiPaneInfo &pane = layoutManager.GetPane(it->window);
        int row = pane.dock_row;
        while (static_cast<int>(rows.size()) <= row)
            rows.push_back(ToolbarRowInfo(0, 0));

        int maxX = rows[row].width + it->window->GetBestSize().x + gripperSize;
        if (maxX > maxWidth)
            nonFitingToolbars.push_back(it->window);
        else
        {
            rows[row].width = maxX;
            pane.Position(rows[row].position++);
        }
    }

    // move the non-fitting toolbars at the bottom
    int lastRow = rows.empty() ? 0 : (rows.size() - 1);
    int position = rows.back().position, maxX = rows.back().width;
    for (std::vector<wxWindow*>::iterator it = nonFitingToolbars.begin(); it != nonFitingToolbars.end(); ++it)
    {
        maxX += (*it)->GetBestSize().x;
        maxX += gripperSize;
        if (maxX > maxWidth)
        {
            position = 0;
            lastRow++;
            maxX = (*it)->GetBestSize().x + gripperSize;
        }
        layoutManager.GetPane(*it).Position(position++).Row(lastRow);
    }
}

// Function which tries to minimize the space used by the toolbars.
// Also it can be used to show toolbars which have gone outside the window.
static void OptimizeToolbars(wxAuiManager &layoutManager, wxWindow *mainFrame)
{
    std::set<ToolbarFitInfo> sorted;
    CollectToolbars(sorted, layoutManager);
    if (sorted.empty())
        return;

    int maxWidth = mainFrame->GetSize().x;
    int lastRow = 0, position = 0, maxX = 0;
    int gripperSize =  layoutManager.GetArtProvider()->GetMetric(wxAUI_DOCKART_GRIPPER_SIZE);

    for (std::set<ToolbarFitInfo>::const_iterator it = sorted.begin(); it != sorted.end(); ++it)
    {
        maxX += it->window->GetBestSize().x;
        maxX += gripperSize;
        if (maxX > maxWidth)
        {
            position = 0;
            lastRow++;
            maxX = it->window->GetBestSize().x + gripperSize;
        }
        layoutManager.GetPane(it->window).Position(position++).Row(lastRow);
    }
}

} // anomymous namespace

void MainFrame::LoadWindowState()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    wxArrayString subs = cfg->EnumerateSubPaths("/main_frame/layout");
    for (size_t i = 0; i < subs.GetCount(); ++i)
    {
        wxString name = cfg->Read("/main_frame/layout/" + subs[i] + "/name");
        wxString layout = cfg->Read("/main_frame/layout/" + subs[i] + "/data");
        wxString layoutMP = cfg->Read("/main_frame/layout/" + subs[i] + "/dataMessagePane");
        SaveViewLayout(name, layout, layoutMP);
    }
    wxString deflayout = cfg->Read("/main_frame/layout/default");
    LoadViewLayout(deflayout);

    DoFixToolbarsLayout();

    // Fit toolbars on load to prevent gaps if toolbar sizes have changed. The most common reason
    // for toolbar change would be change of the size of the icons in the toolbar.
    FitToolbars(m_LayoutManager, this);

    // load manager and messages selected page
    if (m_pPrjManUI->GetNotebook())
        m_pPrjManUI->GetNotebook()->SetSelection(cfg->ReadInt("/main_frame/layout/left_block_selection", 0));

    m_pInfoPane->SetSelection(cfg->ReadInt("/main_frame/layout/bottom_block_selection", 0));

    // Cryogen 23/3/10 wxAuiNotebook can't set it's own tab position once instantiated, for some reason. This code fails in InfoPane::InfoPane().
    // Moved here as this seems like a resonable place to do UI setup. Feel free to move it elsewhere.
    if (cfg->ReadBool("/environment/infopane_tabs_bottom", false))
        m_pInfoPane->SetWindowStyleFlag(m_pInfoPane->GetWindowStyleFlag() | wxAUI_NB_BOTTOM);
}

void MainFrame::LoadWindowSize()
{
#ifndef __WXMAC__
    int x = 0;
    int y = 0;
#else
    int x = 0;
    int y = wxSystemSettings::GetMetric(wxSYS_MENU_Y, this); // make sure it doesn't hide under the menu bar
#endif
    int w = 1000;
    int h = 800;

    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    // obtain display index used last time
    int last_display_index = cfg->ReadInt("/main_frame/layout/display", 0);
    // load window size and position
    wxRect rect(cfg->ReadInt("/main_frame/layout/left",   x),
                cfg->ReadInt("/main_frame/layout/top",    y),
                cfg->ReadInt("/main_frame/layout/width",  w),
                cfg->ReadInt("/main_frame/layout/height", h));

    // maximize if needed
    bool maximized = cfg->ReadBool("/main_frame/layout/maximized", true);
    Maximize(maximized); // toggle

    // set display, size and position
    int display_index_window = wxDisplay::GetFromWindow(this); // C::B usually starts on primary display...
    // ...but try to use display that was used last time, if still available:
    if ((last_display_index>=0) && (last_display_index<static_cast<int>(wxDisplay::GetCount())))
        display_index_window = static_cast<int>(last_display_index);

    int display_index = ((display_index_window>=0) ? display_index_window : 0);

    wxDisplay disp(display_index); // index might be wxNOT_FOUND (=-1) due to GetFromWindow call
    if (maximized)
    {
        rect = disp.GetClientArea(); // apply from display, overriding settings above
        rect.width  -= 100;
        rect.height -= 100;
    }
    else
    {
        // Adjust to actual screen size. This is useful for portable C::B versions,
        // where the window might be out of screen when saving on a two-monitor
        // system an re-opening on a one-monitor system (on Windows, at least).
        wxRect displayRect = disp.GetClientArea();
        if ((displayRect.GetLeft() + displayRect.GetWidth())  < rect.GetLeft())   rect.SetLeft  (displayRect.GetLeft()  );
        if ((displayRect.GetLeft() + displayRect.GetWidth())  < rect.GetRight())  rect.SetRight (displayRect.GetRight() );
        if ((displayRect.GetTop()  + displayRect.GetHeight()) < rect.GetTop())    rect.SetTop   (displayRect.GetTop()   );
        if ((displayRect.GetTop()  + displayRect.GetHeight()) < rect.GetBottom()) rect.SetBottom(displayRect.GetBottom());
    }

    SetSize(rect);
}

void MainFrame::SaveWindowState()
{
    DoCheckCurrentLayoutForChanges(false);

    // first delete all previous layouts, otherwise they might remain
    // if the new amount of layouts is less than the previous, because only the first layouts will be overwritten
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    wxArrayString subs = cfg->EnumerateSubPaths("/main_frame/layout");
    for (size_t i = 0; i < subs.GetCount(); ++i)
        cfg->DeleteSubPath("/main_frame/layout/" + subs[i]);

    int count = 0;
    for (LayoutViewsMap::iterator it = m_LayoutViews.begin(); it != m_LayoutViews.end(); ++it)
    {
        if (it->first.empty())
            continue;

        ++count;
        wxString key = wxString::Format("/main_frame/layout/view%d/", count);
        cfg->Write(key + "name", it->first);
        cfg->Write(key + "data", it->second);

        if (!m_LayoutMessagePane[it->first].empty())
            cfg->Write(key + "dataMessagePane", m_LayoutMessagePane[it->first]);
    }

    // save manager and messages selected page
    if (m_pPrjManUI->GetNotebook())
    {
        int selection = m_pPrjManUI->GetNotebook()->GetSelection();
        cfg->Write("/main_frame/layout/left_block_selection", selection);
    }

    cfg->Write("/main_frame/layout/bottom_block_selection", m_pInfoPane->GetSelection());

    // save display, window size and position
    cfg->Write("/main_frame/layout/display", wxDisplay::GetFromWindow(this));
    if (!IsMaximized() && !IsIconized())
    {
        cfg->Write("/main_frame/layout/left",   GetPosition().x);
        cfg->Write("/main_frame/layout/top",    GetPosition().y);
        cfg->Write("/main_frame/layout/width",  GetSize().x);
        cfg->Write("/main_frame/layout/height", GetSize().y);
    }

    cfg->Write("/main_frame/layout/maximized", IsMaximized());
}

void MainFrame::LoadViewLayout(const wxString& name, bool isTemp)
{
    if (m_LastLayoutName != name && !DoCheckCurrentLayoutForChanges(true))
        return;

    m_LastLayoutIsTemp = isTemp;

    wxString layout(m_LayoutViews[name]);
    wxString layoutMP(m_LayoutMessagePane[name]);
    if (layoutMP.empty())
        layoutMP = m_LayoutMessagePane[gDefaultLayout];

    if (layout.empty())
    {
        layout = m_LayoutViews[gDefaultLayout];
        SaveViewLayout(name, layout, layoutMP, false);
        DoSelectLayout(name);
    }
    else
        DoSelectLayout(name);

    // first load taborder of MessagePane, so LoadPerspective can restore the last selected tab
    m_pInfoPane->LoadTabOrder(layoutMP);

    // We have to force an update here, because the m_LayoutManager.GetAllPanes()
    // would not report correct values if not updated here.

    // Check if translation is active
    if (Manager::Get()->GetConfigManager("app")->ReadBool("/locale/enable"))
    {
        // Yes, translate the captions after loading
        m_LayoutManager.LoadPerspective(layout, false);
        // Fix translations on load (captions are saved in the config file)
        wxAuiPaneInfoArray &panes = m_LayoutManager.GetAllPanes();
        const size_t paneCount = panes.GetCount();
        for (size_t i = 0; i < paneCount; ++i)
            panes[i].caption = wxGetTranslation(panes[i].caption);
    }
    else
    {
        // No, save the english captions and restore them afterwards so
        // undesired translated captions are reset to english
        std::map <wxString, wxString> englishCaptions;
        const wxAuiPaneInfoArray &panes = m_LayoutManager.GetAllPanes();
        const size_t paneCount = panes.GetCount();
        for (size_t i = 0; i < paneCount; ++i)
            englishCaptions[panes[i].name] = panes[i].caption;

        m_LayoutManager.LoadPerspective(layout, false);
        for (std::map <wxString, wxString>::iterator it = englishCaptions.begin(); it != englishCaptions.end(); ++it)
        {
            wxAuiPaneInfo& info = m_LayoutManager.GetPane(it->first);
            if (info.IsOk())
                info.caption = it->second;
        }
    }

    m_LayoutManager.Update();

    // If we load a layout we have to check if the window is on a valid display
    // and has valid size. This can happen if a user moves a layout file from a
    // multi display setup to a single display setup. The size has to be checked
    // because it is possible that the target display has a lower resolution then
    // the source display.
    const wxAuiPaneInfoArray& windowArray = m_LayoutManager.GetAllPanes();
    for (size_t i = 0; i < windowArray.GetCount(); ++i)
    {
        cbFixWindowSizeAndPlace(windowArray.Item(i).frame);
    }

    DoUpdateLayout();

    m_PreviousLayoutName = m_LastLayoutName;
    m_LastLayoutName = name;
    m_LastLayoutData = layout;
    m_LastMessagePaneLayoutData = layoutMP;

    CodeBlocksLayoutEvent evt(cbEVT_SWITCHED_VIEW_LAYOUT);
    evt.layout = name;
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::SaveViewLayout(const wxString& name, const wxString& layout, const wxString& layoutMP, bool select)
{
    if (name.empty())
        return;

    m_LayoutViews[name] = layout;
    m_LayoutMessagePane[name] = layoutMP;
    wxMenu* viewLayouts = nullptr;
    GetMenuBar()->FindItem(idViewLayoutSave, &viewLayouts);
    if (viewLayouts && viewLayouts->FindItem(name) == wxNOT_FOUND)
    {
        int id = wxNewId();
        viewLayouts->InsertCheckItem(viewLayouts->GetMenuItemCount() - 3, id, name, wxString::Format(_("Switch to %s perspective"), name));
        Connect( id,  wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&MainFrame::OnViewLayout);
        m_PluginIDsMap[id] = name;
    }
    if (select)
    {
        DoSelectLayout(name);
        m_LastLayoutName = name;
    }
}

bool MainFrame::LayoutDifferent(const wxString& layout1, const wxString& layout2,
                                const wxString& delimiter)
{
    wxStringTokenizer strTok;
    unsigned long j;

    strTok.SetString(layout1, delimiter);
    wxArrayString arLayout1;
    while(strTok.HasMoreTokens())
    {
        wxStringTokenizer strTokColon(strTok.GetNextToken(), ';');
        while(strTokColon.HasMoreTokens())
        {
            wxString theToken = strTokColon.GetNextToken();
            if (theToken.StartsWith("state="))
            {
                theToken=theToken.Right(theToken.Len() - wxString("state=").Len());
                theToken.ToULong(&j);
                // we filter out the hidden/show state
                theToken = wxString::Format("state=%lu", j & wxAuiPaneInfo::optionHidden);
            }
               arLayout1.Add(theToken);
        }
    }

    strTok.SetString(layout2, delimiter);
    wxArrayString arLayout2;
    while(strTok.HasMoreTokens())
    {
        wxStringTokenizer strTokColon(strTok.GetNextToken(), ';');
        while(strTokColon.HasMoreTokens())
        {
            wxString theToken = strTokColon.GetNextToken();
            if (theToken.StartsWith("state="))
            {
                theToken=theToken.Right(theToken.Len() - wxString("state=").Len());
                theToken.ToULong(&j);
                // we filter out the hidden/show state
                theToken = wxString::Format("state=%lu", j & wxAuiPaneInfo::optionHidden);
            }
               arLayout2.Add(theToken);
        }
    }

    arLayout1.Sort();
    arLayout2.Sort();

    return arLayout1 != arLayout2;
}

bool MainFrame::LayoutMessagePaneDifferent(const wxString& layout1,const wxString& layout2, bool checkSelection)
{
    wxStringTokenizer strTok;
    wxArrayString arLayout1;
    wxArrayString arLayout2;

    strTok.SetString(layout1.BeforeLast('|'), ';');
    while (strTok.HasMoreTokens())
    {
        arLayout1.Add(strTok.GetNextToken());
    }

    strTok.SetString(layout2.BeforeLast('|'), ';');
    while (strTok.HasMoreTokens())
    {
        arLayout2.Add(strTok.GetNextToken());
    }

    if (checkSelection)
    {
        arLayout1.Add(layout1.AfterLast('|'));
        arLayout2.Add(layout2.AfterLast('|'));
    }
    arLayout1.Sort();
    arLayout2.Sort();

    return arLayout1 != arLayout2;
}

bool MainFrame::DoCheckCurrentLayoutForChanges(bool canCancel)
{
    DoFixToolbarsLayout();
    wxString lastlayout = m_LayoutManager.SavePerspective();
    wxString lastmessagepanelayout = m_pInfoPane->SaveTabOrder();

    if (m_LastLayoutName.empty())
        return true;

    bool layoutChanged = false;
    if (LayoutDifferent(lastlayout, m_LastLayoutData, "|"))
        layoutChanged = true;
    else
    {
        ConfigManager *cfg = Manager::Get()->GetConfigManager("message_manager");
        const bool saveSelection = cfg->ReadBool("/save_selection_change_in_mp", true);
        if (LayoutMessagePaneDifferent(lastmessagepanelayout, m_LastMessagePaneLayoutData,
                                       saveSelection))
        {
            layoutChanged = true;
        }
    }

    if (layoutChanged)
    {
        AnnoyingDialog dlg(_("Layout changed"),
                            wxString::Format(_("The perspective '%s' has changed. Do you want to save it?"), m_LastLayoutName),
                            wxART_QUESTION,
                            canCancel ? AnnoyingDialog::YES_NO_CANCEL : AnnoyingDialog::YES_NO,
                            // partial fix for bug 18970 (fix is incomplete to prevent the user from saving 'rtCANCEL')
                            canCancel ? AnnoyingDialog::rtYES : AnnoyingDialog::rtSAVE_CHOICE);
        switch (dlg.ShowModal())
        {
            case AnnoyingDialog::rtYES:
                SaveViewLayout(m_LastLayoutName, lastlayout, lastmessagepanelayout, false);
                break;
            case AnnoyingDialog::rtCANCEL:
                DoSelectLayout(m_LastLayoutName);
                return false;
            default:
                break;
        }
    }
    return true;
}

void MainFrame::DoFixToolbarsLayout()
{
    // The AUI layout system remembers toolbar sizes. In most circumstances we don't want this
    // feature. So we want to disable it and this is what is done in this function.
    // This function has effect after a toolbar size change.
    //
    // To do it we need to do two passes:
    // 1. reset the best/min sizes loaded from the layout file.
    // 2. set new best size
    //
    // The reset operation is needed because wxAUI does nothing when the values
    // for min/best sizes aren't equal to wxDefaultSize.

    wxAuiPaneInfoArray& panes = m_LayoutManager.GetAllPanes();
    for (size_t i=0; i<panes.GetCount(); ++i)
    {
        wxAuiPaneInfo& info = panes[i];
        if (info.IsToolbar() && info.IsShown())
        {
            info.best_size = wxDefaultSize;
            info.min_size = wxDefaultSize;
        }
    }

    // This is needed in order to auto shrink the toolbars to fit the icons
    // with as little space as possible.
    m_LayoutManager.Update();

    for (size_t i=0; i<panes.GetCount(); ++i)
    {
        wxAuiPaneInfo& info = panes[i];
        if (info.IsToolbar() && info.window)
        {
            info.best_size = info.window->GetBestSize();
            info.floating_size = wxDefaultSize;
        }
    }

    // If we don't do this toolbars would be empty during initial startup or after
    // View -> Perspective -> Save current.
    m_LayoutManager.Update();
}

void MainFrame::DoSelectLayout(const wxString& name)
{
    wxMenu* viewLayouts = nullptr;
    GetMenuBar()->FindItem(idViewLayoutSave, &viewLayouts);
    if (viewLayouts)
    {
        wxMenuItemList& items = viewLayouts->GetMenuItems();
        for (size_t i = 0; i < items.GetCount(); ++i)
        {
            if (!items[i]->IsCheckable())
                continue;

            items[i]->Check(items[i]->GetItemLabel().IsSameAs(name));
        }

        if (!m_LastLayoutIsTemp)
            Manager::Get()->GetConfigManager("app")->Write("/main_frame/layout/default", name);
    }
}

void MainFrame::DoAddPluginStatusField(cbPlugin* plugin)
{
    cbStatusBar *sbar = (cbStatusBar *)GetStatusBar();
    if (!sbar)
        return;
    plugin->CreateStatusField(sbar);
    sbar->AdjustFieldsSize();
}

inline void InitToolbar(wxToolBar *tb)
{
    tb->SetInitialSize();
}

ToolbarInfo MainFrame::DoAddPluginToolbar(cbPlugin* plugin)
{
    ToolbarInfo info;
    info.toolbar = Manager::Get()->CreateEmptyToolbar();
    if (plugin->BuildToolBar(info.toolbar))
    {
        info.priority = plugin->GetToolBarPriority();
        SetToolBar(nullptr);
        InitToolbar(info.toolbar);

        // add View->Toolbars menu item for toolbar
        wxMenu* viewToolbars = nullptr;
        GetMenuBar()->FindItem(idViewToolMain, &viewToolbars);
        if (viewToolbars)
        {
            wxMenuItem* item = AddPluginInMenus(viewToolbars, plugin,
                                                (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&MainFrame::OnToggleBar,
                                                -1, true);
            if (item)
            {
                item->Check(true);
                m_PluginsTools[plugin] = info.toolbar;
            }
        }

        const PluginInfo* pluginInfo = Manager::Get()->GetPluginManager()->GetPluginInfo(plugin);
        if (!pluginInfo)
            cbThrow("No plugin info?!?");

        info.paneInfo.Name(pluginInfo->name + "Toolbar").Caption(pluginInfo->title + _(" Toolbar"));
    }
    else
    {
        delete info.toolbar;
        info.toolbar = nullptr;
    }
    return info;
}

void MainFrame::DoAddPlugin(cbPlugin* plugin)
{
    //Manager::Get()->GetLogManager()->DebugLog("Adding plugin: %s", plugin->GetInfo()->name);
    AddPluginInHelpPluginsMenu(plugin);
    if (plugin->GetType() == ptTool)
    {
        AddPluginInPluginsMenu(plugin);
    }
    // offer menu and toolbar space for other plugins
    else
    {
        // menu
        try
        {
            wxMenuBar* mbar = GetMenuBar();
            plugin->BuildMenu(mbar);
        }
        catch (cbException& e)
        {
            e.ShowErrorMessage();
        }
        // Don't load the toolbars during the initial loading of the plugins, this code should be executed
        // only when a single plugins is loaded from the Plugins -> Manager ... window.
        if (!m_ScanningForPlugins)
        {
            // Create the toolbar for the plugin if there is one.
            const ToolbarInfo &toolbarInfo = DoAddPluginToolbar(plugin);
            if (toolbarInfo.toolbar)
            {
                // Place the new toolbar at the bottom of the toolbar pane. Try to reuse the last row
                // if there is enough space in it, otherwise place the new toolbar at a new row.
                int row = 0;
                const wxAuiPaneInfoArray &panes = m_LayoutManager.GetAllPanes();
                for (size_t ii = 0; ii < panes.GetCount(); ++ii)
                {
                    const wxAuiPaneInfo &info = panes[ii];
                    if (info.IsToolbar())
                        row = std::max(row, info.dock_row);
                }
                int minX = 100000, maxX = -100000;
                int position = 0;
                for (size_t ii = 0; ii < panes.GetCount(); ++ii)
                {
                    const wxAuiPaneInfo &info = panes[ii];
                    if (info.IsToolbar() && info.dock_row == row && info.window)
                    {
                        const wxPoint &pt = info.window->GetPosition();
                        minX = std::min(minX, pt.x + info.window->GetSize().x);
                        maxX = std::max(maxX, pt.x + info.window->GetSize().x);
                        position = std::max(position, info.dock_pos);
                    }
                }
                if (maxX + toolbarInfo.toolbar->GetSize().x <= GetSize().x)
                    position++;
                else
                {
                    row++;
                    position = 0;
                }
                wxAuiPaneInfo paneInfo(toolbarInfo.paneInfo);
                m_LayoutManager.AddPane(toolbarInfo.toolbar, paneInfo. ToolbarPane().Top().Row(row).Position(position));
                // Add the event handler for mouse right click
                toolbarInfo.toolbar->Connect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED,
                                             wxCommandEventHandler(MainFrame::OnToolBarRightClick), nullptr, this);

                DoUpdateLayout();
            }
        }
        DoAddPluginStatusField(plugin);
    }
}

bool MainFrame::Open(const wxString& filename, bool addToHistory)
{
    wxFileName fn(filename);
    // really important so that two same files with different names are not loaded twice
    fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG | wxPATH_NORM_SHORTCUT);
    wxString name = fn.GetFullPath();
    LogManager *logger = Manager::Get()->GetLogManager();
    logger->DebugLog("Opening file " + name);
    bool ret = OpenGeneric(name, addToHistory);
    if (!ret)
        logger->LogError(wxString::Format(_("Opening file '%s' failed!"), name));

    return ret;
}

wxString MainFrame::ShowOpenFileDialog(const wxString& caption, const wxString& filter)
{
    wxFileDialog dlg(this,
                     caption,
                     wxEmptyString,
                     wxEmptyString,
                     filter,
                     wxFD_OPEN | compatibility::wxHideReadonly);
    wxString sel;
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
        sel = dlg.GetPath();
    return sel;
}

bool MainFrame::OpenGeneric(const wxString& filename, bool addToHistory)
{
    if (filename.empty())
        return false;

    wxFileName fname(filename); fname.ClearExt(); fname.SetExt("cbp");
    switch ( FileTypeOf(filename) )
    {
        //
        // Workspace
        //
        case ftCodeBlocksWorkspace:
            // verify that it's not the same as the one already open
            if (filename == Manager::Get()->GetProjectManager()->GetWorkspace()->GetFilename())
                return true;
            else
            {
                if ( DoCloseCurrentWorkspace() )
                {
                    wxBusyCursor wait; // loading a worspace can take some time -> showhourglass
                    ShowHideStartPage(true); // hide startherepage, so we can use full tab-range
                    bool ret = Manager::Get()->GetProjectManager()->LoadWorkspace(filename);
                    if (!ret)
                        ShowHideStartPage(); // show/hide startherepage, dependant of settings, if loading failed
                    else if (addToHistory)
                        m_projectsHistory.AddToHistory(Manager::Get()->GetProjectManager()->GetWorkspace()->GetFilename());
                    return ret;
                }
                else
                    return false;
            }
            break;

        //
        // Project
        //
        case ftCodeBlocksProject:
        {
            // Make a check whether the project exists in current workspace
            cbProject* prj = Manager::Get()->GetProjectManager()->IsOpen(fname.GetFullPath());
            if (!prj)
            {
                wxBusyCursor wait; // loading a workspace can take some time -> showhourglass
                return DoOpenProject(filename, addToHistory);
            }
            else
            {
                // NOTE (Morten#1#): A message here will prevent batch-builds from working and is shown sometimes even if correct
                Manager::Get()->GetProjectManager()->SetProject(prj, false);
                return true;
            }
        }
        //
        // Source files
        //
        case ftHeader:
            // fallthrough
        case ftSource:
            // fallthrough
        case ftTemplateSource:
            // fallthrough
        case ftResource:
            return DoOpenFile(filename, addToHistory);
        //
        // For all other files, ask MIME plugin for a suitable handler
        //
        default:
        {
            cbMimePlugin* plugin = Manager::Get()->GetPluginManager()->GetMIMEHandlerForFile(filename);
            // warn user that "Files extension handler" is disabled
            if (!plugin)
            {
                cbMessageBox(wxString::Format(_("Could not open file %s,\nbecause no extension handler could be found."), filename), _("Error"), wxICON_ERROR);
                return false;
            }
            if (plugin->OpenFile(filename) == 0)
            {
                m_filesHistory.AddToHistory(filename);
                return true;
            }
            return false;
        }
    }
    return true;
}

bool MainFrame::DoOpenProject(const wxString& filename, bool addToHistory)
{
//    Manager::Get()->GetLogManager()->DebugLog("Opening project '%s'", filename);
    if (!wxFileExists(filename))
    {
        cbMessageBox(_("The project file does not exist..."), _("Error"), wxICON_ERROR);
        return false;
    }

    ShowHideStartPage(true); // hide startherepage, so we can use full tab-range
    cbProject* prj = Manager::Get()->GetProjectManager()->LoadProject(filename, true);
    if (prj)
    {
        // Target selection wxChoice may be wider than before, fit the toolbars so the compiler
        // toolbar does not cover the one on the right
        FitToolbars(m_LayoutManager, this);
        DoUpdateLayout();
        if (addToHistory)
            m_projectsHistory.AddToHistory(prj->GetFilename());
        return true;
    }
    ShowHideStartPage(); // show/hide startherepage, dependant of settings, if loading failed
    return false;
}

bool MainFrame::DoOpenFile(const wxString& filename, bool addToHistory)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->Open(filename);
    if (ed)
    {
        // Cryogen 24/3/10 Activate the editor after opening. Partial fix for bug #14087.
        ed->Activate();
        if (addToHistory)
            m_filesHistory.AddToHistory(ed->GetFilename());
        return true;
    }
    return false;
}

bool MainFrame::DoCloseCurrentWorkspace()
{
    return Manager::Get()->GetProjectManager()->CloseWorkspace();
}

void MainFrame::DoUpdateStatusBar()
{
    MainStatusBar *sb = dynamic_cast<MainStatusBar*>(GetStatusBar());
    if (sb == nullptr)
        return;
    if (Manager::IsAppShuttingDown())
        return;
    sb->UpdateFields();
}

void MainFrame::DoUpdateEditorStyle(cbAuiNotebook* target, const wxString& prefix, long defaultStyle)
{
    if (!target)
        return;

    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    target->SetTabCtrlHeight(1);

    long nbstyle = cfg->ReadInt("/environment/tabs_style", 0);
    switch (nbstyle)
    {
        case 1: // simple style
            target->SetArtProvider(new wxAuiSimpleTabArt());
            break;

        case 2: // VC 7.1 style
            target->SetArtProvider(new NbStyleVC71());
            break;

        case 3: // Firefox 2 style
            target->SetArtProvider(new NbStyleFF2());
            break;

        default: // default style
            target->SetArtProvider(new wxAuiDefaultTabArt());
    }

    target->SetTabCtrlHeight(-1);

    nbstyle = defaultStyle;
    if (cfg->ReadBool("/environment/" + prefix + "_tabs_bottom"))
        nbstyle |= wxAUI_NB_BOTTOM;

    if (cfg->ReadBool("/environment/tabs_list"))
        nbstyle |= wxAUI_NB_WINDOWLIST_BUTTON;

    target->SetWindowStyleFlag(nbstyle);
}

void MainFrame::DoUpdateEditorStyle()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    long style = wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_MIDDLE_CLICK_CLOSE;
    long closestyle = cfg->ReadInt("/environment/tabs_closestyle", 0);
    switch (closestyle)
    {
        case 1: // current tab
            style |= wxAUI_NB_CLOSE_ON_ACTIVE_TAB;
            break;

        case 2: // right side
            style |= wxAUI_NB_CLOSE_BUTTON;
            break;

        default: // all tabs (default)
            style |= wxAUI_NB_CLOSE_ON_ALL_TABS;
            break;
    }

    cbAuiNotebook* an = Manager::Get()->GetEditorManager()->GetNotebook();

    DoUpdateEditorStyle(an, "editor", style | wxNO_FULL_REPAINT_ON_RESIZE | wxCLIP_CHILDREN);
    if (cfg->ReadBool("/environment/hide_editor_tabs", false))
        an->SetTabCtrlHeight(0);

    an = m_pInfoPane;
    DoUpdateEditorStyle(an, "infopane", style);

    an = m_pPrjManUI->GetNotebook();
    DoUpdateEditorStyle(an, "project", wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_TAB_MOVE);
}

void MainFrame::DoUpdateLayoutColours()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    wxAuiDockArt* art = m_LayoutManager.GetArtProvider();

#ifndef __WXGTK__
    m_LayoutManager.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_ALLOW_ACTIVE_PANE | wxAUI_MGR_TRANSPARENT_DRAG);
#else // #ifndef __WXGTK__
    // workaround for a wxWidgets-bug that makes C::B crash when a floating window gets docked and composite-effects are enabled
    m_LayoutManager.SetFlags((wxAUI_MGR_DEFAULT | wxAUI_MGR_ALLOW_ACTIVE_PANE | wxAUI_MGR_TRANSPARENT_DRAG | wxAUI_MGR_VENETIAN_BLINDS_HINT)& ~wxAUI_MGR_TRANSPARENT_HINT);
#endif // #ifndef __WXGTK__

    art->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE,                 cfg->ReadInt("/environment/aui/border_size", art->GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE)));
    art->SetMetric(wxAUI_DOCKART_SASH_SIZE,                        cfg->ReadInt("/environment/aui/sash_size", art->GetMetric(wxAUI_DOCKART_SASH_SIZE)));
    art->SetMetric(wxAUI_DOCKART_CAPTION_SIZE,                     cfg->ReadInt("/environment/aui/caption_size", art->GetMetric(wxAUI_DOCKART_CAPTION_SIZE)));
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR,            cfg->ReadColour("/environment/aui/active_caption_colour", art->GetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR)));
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR,   cfg->ReadColour("/environment/aui/active_caption_gradient_colour", art->GetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR)));
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR,       cfg->ReadColour("/environment/aui/active_caption_text_colour", art->GetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR)));
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR,          cfg->ReadColour("/environment/aui/inactive_caption_colour", art->GetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR)));
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, cfg->ReadColour("/environment/aui/inactive_caption_gradient_colour", art->GetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR)));
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR,     cfg->ReadColour("/environment/aui/inactive_caption_text_colour", art->GetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR)));

    wxFont font = art->GetFont(wxAUI_DOCKART_CAPTION_FONT);
    font.SetPointSize(cfg->ReadInt("/environment/aui/header_font_size", art->GetFont(wxAUI_DOCKART_CAPTION_FONT).GetPointSize()));
    art->SetFont(wxAUI_DOCKART_CAPTION_FONT, font);

    DoUpdateLayout();
}

void MainFrame::DoUpdateLayout()
{
    if (!m_StartupDone)
        return;

    DoFixToolbarsLayout();
}

void MainFrame::DoUpdateAppTitle()
{
    EditorBase* ed = Manager::Get()->GetEditorManager() ? Manager::Get()->GetEditorManager()->GetActiveEditor() : nullptr;
    cbProject* prj = nullptr;
    if (ed && ed->IsBuiltinEditor())
    {
        ProjectFile* prjf = ((cbEditor*)ed)->GetProjectFile();
        if (prjf)
            prj = prjf->GetParentProject();
    }
    else
        prj = Manager::Get()->GetProjectManager() ? Manager::Get()->GetProjectManager()->GetActiveProject() : nullptr;

    wxString projname;
    wxString edname;
    wxString fulltitle;
    if (ed || prj)
    {
        if (prj)
        {
            if (Manager::Get()->GetProjectManager()->GetActiveProject() == prj)
                projname = wxString(" [") + prj->GetTitle() + "]";
            else
                projname = wxString(" (") + prj->GetTitle() + ")";
        }
        if (ed)
            edname = ed->GetTitle();
        fulltitle = edname + projname;
        if (!fulltitle.empty())
            fulltitle.Append(" - ");
    }

    fulltitle.Append(appglobals::AppName);
    fulltitle.Append(" ");
    fulltitle.Append(appglobals::AppVersion);
    SetTitle(fulltitle);
}

void MainFrame::ShowHideStartPage(bool forceHasProject, int forceState)
{
//    Manager::Get()->GetLogManager()->LogWarning(_("Toggling start page"));
    if (Manager::IsBatchBuild())
        return;

    // we use the 'forceHasProject' param because when a project is opened
    // the EVT_PROJECT_OPEN event is fired *before* ProjectManager::GetProjects()
    // and ProjectManager::GetActiveProject() are updated...

    if (m_InitiatedShutdown)
    {
        EditorBase* sh = Manager::Get()->GetEditorManager()->GetEditor(GetStartHereTitle());
        if (sh)
            sh->Destroy();
        return;
    }

    bool show = !forceHasProject &&
                Manager::Get()->GetProjectManager()->GetProjects()->GetCount() == 0 &&
                Manager::Get()->GetConfigManager("app")->ReadBool("/environment/start_here_page", true);

    if (forceState<0)
        show = false;
    if (forceState>0)
        show = true;

    EditorBase* sh = Manager::Get()->GetEditorManager()->GetEditor(GetStartHereTitle());
    if (show)
    {
        if (!sh)
        {
            sh = new StartHerePage(this, m_projectsHistory, m_filesHistory,
                                   Manager::Get()->GetEditorManager()->GetNotebook());
        }
        else
            static_cast<StartHerePage*>(sh)->Reload();
    }
    else if (!show && sh)
        sh->Destroy();

    DoUpdateAppTitle();
}

void MainFrame::ShowHideScriptConsole()
{
    if (Manager::IsBatchBuild())
        return;
    bool isVis = IsWindowReallyShown(m_pScriptConsole);
    CodeBlocksDockEvent evt(isVis ? cbEVT_HIDE_DOCK_WINDOW : cbEVT_SHOW_DOCK_WINDOW);
    evt.pWindow = m_pScriptConsole;
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::OnStartHereLink(wxCommandEvent& event)
{
    wxCommandEvent evt;
    evt.SetId(idFileNewProject);
    wxString link = event.GetString();
    if (link.IsSameAs("CB_CMD_NEW_PROJECT"))
        OnFileNewWhat(evt);
    else if (link.IsSameAs("CB_CMD_OPEN_PROJECT"))
        DoOnFileOpen(true);
//    else if (link.IsSameAs("CB_CMD_CONF_ENVIRONMENT"))
//        OnSettingsEnvironment(evt);
//    else if (link.IsSameAs("CB_CMD_CONF_EDITOR"))
//        Manager::Get()->GetEditorManager()->Configure();
//    else if (link.IsSameAs("CB_CMD_CONF_COMPILER"))
//        OnSettingsCompilerDebugger(evt);
    else if (link.StartsWith("CB_CMD_OPEN_HISTORY_"))
    {
        RecentItemsList *recent;
        recent = link.StartsWith("CB_CMD_OPEN_HISTORY_PROJECT_") ? &m_projectsHistory : &m_filesHistory;
        unsigned long item;
        link.AfterLast('_').ToULong(&item);
        --item;
        const wxString &filename = recent->GetHistoryFile(item);
        if (!filename.empty())
        {
            if ( !OpenGeneric(filename, true) )
                recent->AskToRemoveFileFromHistory(item);
        }
    }
    else if (link.StartsWith("CB_CMD_DELETE_HISTORY_"))
    {
        RecentItemsList *recent;
        recent = link.StartsWith("CB_CMD_DELETE_HISTORY_PROJECT_") ? &m_projectsHistory : &m_filesHistory;
        unsigned long item;
        link.AfterLast('_').ToULong(&item);
        --item;
        recent->AskToRemoveFileFromHistory(item, false);
    }
    else if (link.IsSameAs("CB_CMD_TIP_OF_THE_DAY"))
        ShowTips(true);
}

void MainFrame::InitializeRecentFilesHistory()
{
    m_filesHistory.Initialize();
    m_projectsHistory.Initialize();
}

void MainFrame::TerminateRecentFilesHistory()
{
    m_filesHistory.TerminateHistory();
    m_projectsHistory.TerminateHistory();
}

wxString MainFrame::GetEditorDescription(EditorBase* eb)
{
    wxString descr;
    cbProject* prj = nullptr;
    if (eb && eb->IsBuiltinEditor())
    {
        ProjectFile* prjf = ((cbEditor*)eb)->GetProjectFile();
        if (prjf)
            prj = prjf->GetParentProject();
    }
    else
        prj = Manager::Get()->GetProjectManager() ? Manager::Get()->GetProjectManager()->GetActiveProject() : nullptr;
    if (prj)
    {
        descr = wxString(_("Project: ")) + "<b>" + prj->GetTitle() + "</b>";
        if (Manager::Get()->GetProjectManager()->GetActiveProject() == prj)
            descr += wxString(_(" (Active)"));
        descr += wxString("<br>");
    }
    if (eb)
        descr += eb->GetFilename();
    return descr;
}

////////////////////////////////////////////////////////////////////////////////
// event handlers
////////////////////////////////////////////////////////////////////////////////

void MainFrame::OnPluginsExecuteMenu(wxCommandEvent& event)
{
    const wxString pluginName(m_PluginIDsMap[event.GetId()]);
    if (!pluginName.empty())
        Manager::Get()->GetPluginManager()->ExecutePlugin(pluginName);
    else
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("No plugin found for ID %d", event.GetId()));
}

void MainFrame::OnHelpPluginMenu(wxCommandEvent& event)
{
    const wxString pluginName(m_PluginIDsMap[event.GetId()]);
    if (!pluginName.empty())
    {
        const PluginInfo* pi = Manager::Get()->GetPluginManager()->GetPluginInfo(pluginName);
        if (!pi)
        {
            Manager::Get()->GetLogManager()->DebugLog("No plugin info for " + pluginName);
            return;
        }

        dlgAboutPlugin dlg(this, pi);
        PlaceWindow(&dlg);
        dlg.ShowModal();
    }
    else
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("No plugin found for ID %d", event.GetId()));
}

void MainFrame::OnFileNewWhat(wxCommandEvent& event)
{
    int id = event.GetId();
    if (id != idFileNewEmpty)
    {
        // wizard-based

        TemplateOutputType tot = totProject;
        if      (id == idFileNewProject) tot = totProject;
        else if (id == idFileNewTarget)  tot = totTarget;
        else if (id == idFileNewFile)    tot = totFiles;
        else if (id == idFileNewCustom)  tot = totCustom;
        else if (id == idFileNewUser)    tot = totUser;
        else                             return;

        wxString filename;
        cbProject* prj = TemplateManager::Get()->New(tot, &filename);
        // verify that the open files are still in sync
        // the new file might have overwritten an existing one)
        Manager::Get()->GetEditorManager()->CheckForExternallyModifiedFiles();

        // If both are empty it means that the wizard has failed
        if (!prj && filename.empty())
            return;

        // Send the new project event
        CodeBlocksEvent evtNew(cbEVT_PROJECT_NEW, 0, prj);
        Manager::Get()->GetPluginManager()->NotifyPlugins(evtNew);

        if (prj)
        {
            prj->Save();
            prj->SaveAllFiles();
        }

        if (!filename.empty())
        {
            if (prj)
                m_projectsHistory.AddToHistory(filename);
            else
                m_filesHistory.AddToHistory(filename);
        }
        if (prj && tot == totProject) // Created project should be parsed
        {
            CodeBlocksEvent evtOpen(cbEVT_PROJECT_OPEN, 0, prj);
            Manager::Get()->GetPluginManager()->NotifyPlugins(evtOpen);
        }
        return;
    }

    // new empty file quick shortcut code below

    cbProject* project = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (project)
        wxSetWorkingDirectory(project->GetBasePath());
    cbEditor* ed = Manager::Get()->GetEditorManager()->New();

    // initially start change-collection if configured on empty files
    if (ed)
        ed->GetControl()->SetChangeCollection(Manager::Get()->GetConfigManager("editor")->ReadBool("/margin/use_changebar", true));

    if (ed && ed->IsOK())
        m_filesHistory.AddToHistory(ed->GetFilename());

    if (!ed || !project)
        return;

    if (cbMessageBox(_("Do you want to add this new file in the active project (has to be saved first)?"),
                    _("Add file to project"),
                    wxYES_NO | wxICON_QUESTION) == wxID_YES &&
        ed->SaveAs() && ed->IsOK())
    {
        wxArrayInt targets;
        if (Manager::Get()->GetProjectManager()->AddFileToProject(ed->GetFilename(), project, targets) != 0)
        {
            ProjectFile* pf = project->GetFileByFilename(ed->GetFilename(), false);
            ed->SetProjectFile(pf);
            m_pPrjManUI->RebuildTree();
        }
    }
    // verify that the open files are still in sync
    // the new file might have overwritten an existing one)
    Manager::Get()->GetEditorManager()->CheckForExternallyModifiedFiles();
}

bool MainFrame::OnDropFiles(wxCoord /*x*/, wxCoord /*y*/, const wxArrayString& files)
{
    bool success = true; // Safe case initialisation

    // first check to see if a workspace is passed. If so, only this will be loaded
    wxString foundWorkspace;
    for (unsigned int i = 0; i < files.GetCount(); ++i)
    {
        FileType ft = FileTypeOf(files[i]);
        if (ft == ftCodeBlocksWorkspace || ft == ftMSVC6Workspace || ft == ftMSVC7Workspace)
        {
            foundWorkspace = files[i];
            break;
        }
    }

    if (!foundWorkspace.empty())
      success &= OpenGeneric(foundWorkspace);
    else
    {
        wxBusyCursor useless;
        for (unsigned int i = 0; i < files.GetCount(); ++i)
          success &= OpenGeneric(files[i]);
    }
    return success;
}

void MainFrame::OnFileNew(cb_unused wxCommandEvent& event)
{
    wxMenu* popup = nullptr;
    wxMenuBar* bar = GetMenuBar();
    if (!bar)
        return;

    bar->FindItem(idFileNewProject, &popup);
    if (popup)
    {
        popup = CopyMenu(popup);
        if (popup)
        {
            PopupMenu(popup); // this will lead us in OnFileNewWhat() - the meat is there ;)
            delete popup;
        }
    }
}

// in case we are opening a project (bProject == true) we do not want to interfere
// with 'last type of files' (probably the call was open (existing) project on the
// start here page --> so we know it's a project --> set the filter accordingly
// but as said don't force the 'last used type of files' to change, that should
// only change when an open file is carried out (so (source) file <---> project (file) )
// TODO : when regular file open and user manually sets filter to project files --> will change
//      the last type : is that expected behaviour ???
void MainFrame::DoOnFileOpen(bool bProject)
{
    wxString Filters = FileFilters::GetFilterString();
    // the value returned by GetIndexForFilterAll() is updated by GetFilterString()
    int StoredIndex = FileFilters::GetIndexForFilterAll();
    wxString Path;
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    if (cfg)
    {
        if (!bProject)
        {
            const wxString Filter(cfg->Read("/file_dialogs/file_new_open/filter"));
            if (!Filter.empty())
                FileFilters::GetFilterIndexFromName(Filters, Filter, StoredIndex);

            Path = cfg->Read("/file_dialogs/file_new_open/directory", Path);
        }
        else
            FileFilters::GetFilterIndexFromName(Filters, _("Code::Blocks project/workspace files"), StoredIndex);
    }

    wxFileDialog dlg(this,
                     _("Open file"),
                     Path,
                     wxEmptyString,
                     Filters,
                     wxFD_OPEN | wxFD_MULTIPLE | compatibility::wxHideReadonly);

    dlg.SetFilterIndex(StoredIndex);

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        // store the last used filter and directory
        // as said : don't do this in case of an 'open project'
        if (cfg && !bProject)
        {
            int Index = dlg.GetFilterIndex();
            wxString Filter;
            if (FileFilters::GetFilterNameFromIndex(Filters, Index, Filter))
                cfg->Write("/file_dialogs/file_new_open/filter", Filter);

            cfg->Write("/file_dialogs/file_new_open/directory", dlg.GetDirectory());
        }

        wxArrayString files;
        dlg.GetPaths(files);
        OnDropFiles(0,0,files);
    }
}

void MainFrame::OnFileOpen(cb_unused wxCommandEvent& event)
{
    DoOnFileOpen(false); // through file menu (not sure if we are opening a project)
}

void MainFrame::OnFileReopenProject(wxCommandEvent& event)
{
    size_t id = event.GetId() - wxID_CBFILE17;
    wxString fname = m_projectsHistory.GetHistoryFile(id);
    if (!OpenGeneric(fname, true))
        m_projectsHistory.AskToRemoveFileFromHistory(id);
}

void MainFrame::OnFileOpenRecentProjectClearHistory(cb_unused wxCommandEvent& event)
{
    m_projectsHistory.ClearHistory();
}

void MainFrame::OnFileReopen(wxCommandEvent& event)
{
    size_t id = event.GetId() - wxID_CBFILE01;
    wxString fname = m_filesHistory.GetHistoryFile(id);
    if (!OpenGeneric(fname, true))
        m_filesHistory.AskToRemoveFileFromHistory(id);
}

void MainFrame::OnFileOpenRecentClearHistory(cb_unused wxCommandEvent& event)
{
    m_filesHistory.ClearHistory();
}

void MainFrame::OnFileSave(cb_unused wxCommandEvent& event)
{
    if (!Manager::Get()->GetEditorManager()->SaveActive())
    {
        wxString msg;
        msg.Printf(_("File %s could not be saved..."), Manager::Get()->GetEditorManager()->GetActiveEditor()->GetFilename());
        cbMessageBox(msg, _("Error saving file"), wxICON_ERROR);
    }
    DoUpdateStatusBar();
}

void MainFrame::OnFileSaveAs(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->SaveActiveAs();
    DoUpdateStatusBar();
}

void MainFrame::OnFileSaveProject(cb_unused wxCommandEvent& event)
{
    // no need to call SaveActiveProjectAs(), because this is handled in cbProject::Save()
    ProjectManager *prjManager = Manager::Get()->GetProjectManager();
    if (prjManager->SaveActiveProject())
        m_projectsHistory.AddToHistory(prjManager->GetActiveProject()->GetFilename());
    DoUpdateStatusBar();
    DoUpdateAppTitle();
}

void MainFrame::OnFileSaveProjectAs(cb_unused wxCommandEvent& event)
{
    ProjectManager *prjManager = Manager::Get()->GetProjectManager();
    if (prjManager->SaveActiveProjectAs())
        m_projectsHistory.AddToHistory(prjManager->GetActiveProject()->GetFilename());
    DoUpdateStatusBar();
    DoUpdateAppTitle();
}

void MainFrame::OnFileSaveAll(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetConfigManager("app")->Flush();
    Manager::Get()->GetEditorManager()->SaveAll();
    ProjectManager *prjManager = Manager::Get()->GetProjectManager();
    prjManager->SaveAllProjects();

    if (prjManager->GetWorkspace()->GetModified()
        && !prjManager->GetWorkspace()->IsDefault()
        && prjManager->SaveWorkspace())
    {
        m_projectsHistory.AddToHistory(prjManager->GetWorkspace()->GetFilename());
    }

    DoUpdateStatusBar();
    DoUpdateAppTitle();
}

void MainFrame::OnFileSaveProjectTemplate(cb_unused wxCommandEvent& event)
{
    TemplateManager::Get()->SaveUserTemplate(Manager::Get()->GetProjectManager()->GetActiveProject());
}

void MainFrame::OnFileCloseProject(cb_unused wxCommandEvent& event)
{
    // we 're not actually shutting down here, but we want to check if the
    // active project is still opening files (still busy)
    if (!ProjectManager::CanShutdown() || !EditorManager::CanShutdown())
    {
        wxBell();
        return;
    }
    Manager::Get()->GetProjectManager()->CloseActiveProject();
    DoUpdateStatusBar();
}

void MainFrame::OnFileImportProjectDevCpp(cb_unused wxCommandEvent& event)
{
    OpenGeneric(ShowOpenFileDialog(_("Import Dev-C++ project"), FileFilters::GetFilterString('.' + FileFilters::DEVCPP_EXT)), false);
}

void MainFrame::OnFileImportProjectMSVC(cb_unused wxCommandEvent& event)
{
    OpenGeneric(ShowOpenFileDialog(_("Import MS Visual C++ 6.0 project"), FileFilters::GetFilterString('.' + FileFilters::MSVC6_EXT)), false);
}

void MainFrame::OnFileImportProjectMSVCWksp(cb_unused wxCommandEvent& event)
{
    OpenGeneric(ShowOpenFileDialog(_("Import MS Visual C++ 6.0 workspace"), FileFilters::GetFilterString('.' + FileFilters::MSVC6_WORKSPACE_EXT)), false);
}

void MainFrame::OnFileImportProjectMSVS(cb_unused wxCommandEvent& event)
{
    OpenGeneric(ShowOpenFileDialog(_("Import MS Visual Studio 7.0+ project"), FileFilters::GetFilterString('.' + FileFilters::MSVC7_EXT)), false);
}

void MainFrame::OnFileImportProjectMSVSWksp(cb_unused wxCommandEvent& event)
{
    OpenGeneric(ShowOpenFileDialog(_("Import MS Visual Studio 7.0+ solution"), FileFilters::GetFilterString('.' + FileFilters::MSVC7_WORKSPACE_EXT)), false);
}

void MainFrame::OnFileOpenDefWorkspace(cb_unused wxCommandEvent& event)
{
    ProjectManager *pman = Manager::Get()->GetProjectManager();
    if (!pman->GetWorkspace()->IsDefault() && !pman->LoadWorkspace())
    {
        // do not add the default workspace in recent projects list
        // it's always one menu click away
        cbMessageBox(_("Can't open default workspace (file exists?)"), _("Warning"), wxICON_WARNING);
    }
}

void MainFrame::OnFileSaveWorkspace(cb_unused wxCommandEvent& event)
{
    ProjectManager *pman = Manager::Get()->GetProjectManager();
    if (pman->SaveWorkspace())
        m_projectsHistory.AddToHistory(pman->GetWorkspace()->GetFilename());
}

void MainFrame::OnFileSaveWorkspaceAs(cb_unused wxCommandEvent& event)
{
    ProjectManager *pman = Manager::Get()->GetProjectManager();
    if (pman->SaveWorkspaceAs(""))
        m_projectsHistory.AddToHistory(pman->GetWorkspace()->GetFilename());
}

void MainFrame::OnFileCloseWorkspace(cb_unused wxCommandEvent& event)
{
    DoCloseCurrentWorkspace();
}

void MainFrame::OnFileClose(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->CloseActive();
    DoUpdateStatusBar();
    Refresh();
}

void MainFrame::OnFileCloseAll(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->CloseAll();
    DoUpdateStatusBar();
}

void MainFrame::OnFileNext(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->ActivateNext();
    DoUpdateStatusBar();
}

void MainFrame::OnFilePrev(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->ActivatePrevious();
    DoUpdateStatusBar();
}

void MainFrame::OnFilePrint(cb_unused wxCommandEvent& event)
{
    PrintDialog dlg(this);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
        Manager::Get()->GetEditorManager()->Print(dlg.GetPrintScope(), dlg.GetPrintColourMode(), dlg.GetPrintLineNumbers());
}

void MainFrame::OnFileQuit(cb_unused wxCommandEvent& event)
{
    Close(false);
}

void MainFrame::OnEraseBackground(wxEraseEvent& event)
{
    // for flicker-free display
    event.Skip();
}

void MainFrame::OnApplicationClose(wxCloseEvent& event)
{
    if (m_InitiatedShutdown)
        return;

    CodeBlocksEvent evt(cbEVT_APP_START_SHUTDOWN);
    Manager::Get()->ProcessEvent(evt);

    m_InitiatedShutdown = true;
    Manager::BlockYields(true);

    {
        // Check if any compiler plugin is building and ask the user if he/she wants to stop it.
        if (cbHasRunningCompilers(Manager::Get()->GetPluginManager()))
        {
            int result = cbMessageBox(_("Code::Blocks is currently compiling or running an application.\n"
                                        "Do you want to stop the action and exit?"),
                                      _("Question"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                                      this);
            if (result == wxID_YES)
                cbStopRunningCompilers(Manager::Get()->GetPluginManager());
            else
            {
                event.Veto();
                wxBell();
                m_InitiatedShutdown = false;
                Manager::BlockYields(false);
                return;
            }
        }
    }

    if (!ProjectManager::CanShutdown() || !EditorManager::CanShutdown())
    {
        event.Veto();
        wxBell();
        m_InitiatedShutdown = false;
        Manager::BlockYields(false);
        return;
    }

    if (!DoCloseCurrentWorkspace())
    {
        event.Veto();
        m_InitiatedShutdown = false;
        Manager::BlockYields(false);
        return;
    }

    Manager::SetAppShuttingDown(true);

    Manager::Get()->GetLogManager()->DebugLog("Deinitializing plugins...");
    CodeBlocksEvent evtShutdown(cbEVT_APP_START_SHUTDOWN);
    Manager::Get()->ProcessEvent(evtShutdown);
    Manager::Yield();

    if (!Manager::IsBatchBuild())
        SaveWindowState();

    if (m_pPrjManUI->GetNotebook())
        m_LayoutManager.DetachPane(m_pPrjManUI->GetNotebook());
    m_LayoutManager.DetachPane(m_pInfoPane);
    m_LayoutManager.DetachPane(Manager::Get()->GetEditorManager()->GetNotebook());

    #if defined ( __WIN32__ ) || defined ( _WIN64 )
    // For Windows, close shown floating windows before shutdown to avoid hangs in Hide() and
    // crashes in Manager::Shutdown();
    wxAuiPaneInfoArray& all_panes = m_LayoutManager.GetAllPanes();
    for(size_t ii = 0; ii < all_panes.Count(); ++ii)
    {
        wxAuiPaneInfo paneInfo = all_panes[ii];
        if (paneInfo.IsShown() and paneInfo.IsFloating())
            m_LayoutManager.ClosePane(paneInfo);
    }
    #endif

    m_LayoutManager.UnInit();

    TerminateRecentFilesHistory();

    // remove all other event handlers from this window
    // this stops it from crashing, when no plugins are loaded
    while (GetEventHandler() != this)
        PopEventHandler(false);

    // Hide the window
    Hide();

    if (!Manager::IsBatchBuild())
    {
        m_pInfoPane->Destroy();
        m_pInfoPane = nullptr;
    }

    // Disconnect the mouse right click event handler for toolbars, this should be done before the plugin is
    // unloaded in Manager::Shutdown().
    PluginToolbarsMap::iterator it;
    for( it = m_PluginsTools.begin(); it != m_PluginsTools.end(); ++it )
    {
        wxToolBar* toolbar = it->second;
        if (toolbar)//Disconnect the mouse right click event handler before the toolbar is destroyed
        {
            bool result = toolbar->Disconnect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED, wxCommandEventHandler(MainFrame::OnToolBarRightClick));
            cbAssert(result);
        }
    }

    Manager::Shutdown(); // Shutdown() is not Free(), Manager is automatically destroyed at exit

    Destroy();
}

void MainFrame::OnEditSwapHeaderSource(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetEditorManager()->SwapActiveHeaderSource();
    DoUpdateStatusBar();
}

void MainFrame::OnEditGotoMatchingBrace(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GotoMatchingBrace();
}

void MainFrame::OnEditBookmarksToggle(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed && ed->IsBuiltinEditor())
        static_cast<cbEditor*>(ed)->ToggleBookmark();
}

void MainFrame::OnEditBookmarksNext(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed && ed->IsBuiltinEditor())
        static_cast<cbEditor*>(ed)->GotoNextBookmark();
}

void MainFrame::OnEditBookmarksPrevious(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed && ed->IsBuiltinEditor())
        static_cast<cbEditor*>(ed)->GotoPreviousBookmark();
}

void MainFrame::OnEditBookmarksClearAll(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed && ed->IsBuiltinEditor())
        static_cast<cbEditor*>(ed)->ClearAllBookmarks();
}

void MainFrame::OnEditUndo(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed && ed->CanUndo())
    {
        cbEditor* cbEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(ed);
        if (cbEd && cbEd->GetControl()->AutoCompActive())
            cbEd->GetControl()->AutoCompCancel();
        ed->Undo();
    }
}

void MainFrame::OnEditRedo(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed)
        ed->Redo();
}

void MainFrame::OnEditClearHistory(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed)
        ed->ClearHistory();
}

void MainFrame::OnEditCopy(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed)
        ed->Copy();
}

void MainFrame::OnEditCut(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed)
        ed->Cut();
}

void MainFrame::OnEditPaste(cb_unused wxCommandEvent& event)
{
    EditorBase* ed = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (ed)
        ed->Paste();
}

void MainFrame::OnEditParaUp(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ParaUp();
}

void MainFrame::OnEditParaUpExtend(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ParaUpExtend();
}

void MainFrame::OnEditParaDown(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ParaDown();
}

void MainFrame::OnEditParaDownExtend(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ParaDownExtend();
}

void MainFrame::OnEditWordPartLeft(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->WordPartLeft();
}

void MainFrame::OnEditWordPartLeftExtend(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->WordPartLeftExtend();
}

void MainFrame::OnEditWordPartRight(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->WordPartRight();
}

void MainFrame::OnEditWordPartRightExtend(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->WordPartRightExtend();
}

void MainFrame::OnEditZoomIn(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ZoomIn();
}

void MainFrame::OnEditZoomOut(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->ZoomOut();
}

void MainFrame::OnEditZoomReset(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->SetZoom(0);
}

void MainFrame::OnEditLineCut(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LineCut();
}

void MainFrame::OnEditLineDelete(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LineDelete();
}

void MainFrame::OnEditLineDuplicate(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LineDuplicate();
}

void MainFrame::OnEditLineTranspose(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LineTranspose();
}

void MainFrame::OnEditLineCopy(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LineCopy();
}

void MainFrame::OnEditLinePaste(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        //We want to undo all in one step
        ed->GetControl()->BeginUndoAction();

        int pos = ed->GetControl()->GetCurrentPos();
        int line = ed->GetControl()->LineFromPosition(pos);
        ed->GetControl()->GotoLine(line);
        int column = pos - ed->GetControl()->GetCurrentPos();
        ed->GetControl()->Paste();
        pos = ed->GetControl()->GetCurrentPos();
        ed->GetControl()->GotoPos(pos+column);

        ed->GetControl()->EndUndoAction();
    }
}

void MainFrame::OnEditLineMove(wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    // TODO (mortenmacfly##): Exclude rectangle selection here or not? What behaviour the majority of users expect here???
    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc || /*stc->SelectionIsRectangle() ||*/ (stc->GetSelections() > 1))
        return;
    if (stc->GetLineCount() < 3) // why are you trying to move lines anyways?
        return;

    // note that these function calls are a much simpler, however
    // they cause text to unnecessarily be marked as modified
//    if (event.GetId() == idEditLineUp)
//        stc->MoveSelectedLinesUp();
//    else
//        stc->MoveSelectedLinesDown();

    int startPos = stc->PositionFromLine(stc->LineFromPosition(stc->GetSelectionStart()));
    int endPos   = stc->LineFromPosition(stc->GetSelectionEnd()); // is line
    if (   stc->GetSelectionEnd() == stc->PositionFromLine(endPos)          // end is in first column
        && stc->PositionFromLine(endPos) != stc->GetLineEndPosition(endPos) // this line has text
        && endPos > stc->LineFromPosition(startPos) )                       // start and end are on different lines
    {
        --endPos; // do not unexpectedly select another line
    }
    const bool isLastLine = (endPos == stc->GetLineCount() - 1);
    // warning: stc->GetLineEndPosition(endPos) yields strange results for CR LF (see bug 18892)
    endPos = (  isLastLine ? stc->GetLineEndPosition(endPos)
              : stc->PositionFromLine(endPos + 1) - 1 ); // is position
    if (event.GetId() == idEditLineUp)
    {
        if (stc->LineFromPosition(startPos) < 1)
            return; // cannot move up (we are at the top), exit
        stc->BeginUndoAction();
        const int offset     = (isLastLine ? startPos - stc->GetLineEndPosition(stc->LineFromPosition(startPos) - 1) : 0);
        const int lineLength = startPos - stc->PositionFromLine(stc->LineFromPosition(startPos) - 1);
        const wxString line  = stc->GetTextRange(startPos - lineLength - offset,
                                                 startPos - offset);
        stc->InsertText(endPos + (isLastLine ? 0 : 1), line);
        // warning: line.length() != lineLength if multibyte characters are used
        stc->DeleteRange(startPos - lineLength, lineLength);
        startPos -= lineLength;
        endPos   -= lineLength;
        stc->EndUndoAction();
    }
    else // event.GetId() == idEditLineDown
    {
        if (isLastLine)
            return; // cannot move down (we are at the bottom), exit
        stc->BeginUndoAction();
        const int lineLength = stc->PositionFromLine(stc->LineFromPosition(endPos + 1) + 1) - endPos - 1;
        const wxString line  = stc->GetTextRange(endPos + 1,
                                                 endPos + 1 + lineLength);
        stc->InsertText(startPos, line);
        // warning: line.length() != lineLength if multibyte characters are used
        startPos += lineLength;
        endPos   += lineLength;
        stc->DeleteRange(endPos + 1, lineLength);
        stc->EndUndoAction();
    }
    stc->SetSelectionVoid(startPos, endPos);
}

void MainFrame::OnEditUpperCase(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->UpperCase();
}

void MainFrame::OnEditLowerCase(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GetControl()->LowerCase();
}

void MainFrame::OnEditInsertNewLine(wxCommandEvent& event)
{
    OnEditGotoLineEnd(event);
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        if (stc->AutoCompActive())
            stc->AutoCompCancel();
        stc->NewLine();
    }
}

void MainFrame::OnEditGotoLineEnd(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        if (stc->AutoCompActive())
            stc->AutoCompCancel();
        stc->LineEnd();
    }
}

static void InsertNewLine(bool below)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        stc->BeginUndoAction();
        if (stc->AutoCompActive())
            stc->AutoCompCancel();

        if (below)
        {
            stc->LineEndDisplay();
            int pos = stc->GetCurrentPos();
            stc->InsertText(pos, GetEOLStr(stc->GetEOLMode()));
            stc->LineDown();
        }
        else
        {
            stc->HomeDisplay();
            int pos = stc->GetCurrentPos();
            stc->InsertText(pos, GetEOLStr(stc->GetEOLMode()));
            stc->EndUndoAction();
        }
        stc->EndUndoAction();
    }
}

void MainFrame::OnEditInsertNewLineBelow(cb_unused wxCommandEvent& event)
{
    InsertNewLine(true);
}

void MainFrame::OnEditInsertNewLineAbove(cb_unused wxCommandEvent& event)
{
    InsertNewLine(false);
}

void MainFrame::OnEditSelectAll(cb_unused wxCommandEvent& event)
{
    EditorBase* eb = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (eb)
        eb->SelectAll();
}

namespace
{

struct EditorSelection
{
    long caret, anchor;

    bool Empty() const { return caret == anchor; }
    bool IsReversed() const { return caret < anchor; }

    long GetStart() const { return std::min(caret, anchor); }
    long GetEnd() const { return std::max(caret, anchor); }

    bool Contains(const EditorSelection &selection) const
    {
        return !(GetEnd() < selection.GetStart() || GetStart() > selection.GetEnd());
    }
};

bool SelectNext(cbStyledTextCtrl *control, const wxString &selectedText, long selectionEnd, bool reversed)
{
    // always match case and try to match whole words if they have no special characters
    int flag = wxSCI_FIND_MATCHCASE;
    if (selectedText.find_first_of(";:\"'`~@#$%^,-+*/\\=|!?&*(){}[]") == wxString::npos)
        flag |= wxSCI_FIND_WHOLEWORD;

    int endPos = 0; // we need this to work properly with multibyte characters
    int eof = control->GetLength();
    int pos = control->FindText(selectionEnd, eof, selectedText, flag, &endPos);
    if (pos != wxSCI_INVALID_POSITION)
    {
        control->SetAdditionalSelectionTyping(true);
        control->SetMultiPaste(true);
        control->IndicatorClearRange(pos, endPos - pos);
        if (reversed)
            control->AddSelection(pos, endPos);
        else
            control->AddSelection(endPos, pos);
        control->MakeNearbyLinesVisible(control->LineFromPosition(pos));
        return true;
    }
    else
    {
        InfoWindow::Display(_("Select Next Occurrence"), _("No more available"));
        return false;
    }
}

bool GetSelectionInEditor(EditorSelection &selection, cbStyledTextCtrl *control)
{
    int main = control->GetMainSelection();
    int count = control->GetSelections();
    if (main >=0 && main < count)
    {
        selection.caret = control->GetSelectionNCaret(main);
        selection.anchor = control->GetSelectionNAnchor(main);
        return true;
    }
    else
        return false;
}

} // anonymous namespace

void MainFrame::OnEditSelectNext(cb_unused wxCommandEvent& event)
{
    EditorBase* eb = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (!eb || !eb->IsBuiltinEditor())
        return;
    cbStyledTextCtrl *control = static_cast<cbEditor*>(eb)->GetControl();

    EditorSelection selection;
    if (!GetSelectionInEditor(selection, control))
        return;

    if (!selection.Empty())
    {
        const wxString &selectedText(control->GetTextRange(selection.GetStart(), selection.GetEnd()));
        SelectNext(control, selectedText, selection.GetEnd(), selection.IsReversed());
    }
    else
    {
        // Select word at the cursor position if there is nothing selected.
        const int start = control->WordStartPosition(selection.caret, true);
        const int end = control->WordEndPosition(selection.caret, true);
        control->SetSelection(start, end);
    }
}

void MainFrame::OnEditSelectNextSkip(cb_unused wxCommandEvent& event)
{
    EditorBase* eb = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if (!eb || !eb->IsBuiltinEditor())
        return;
    cbStyledTextCtrl *control = static_cast<cbEditor*>(eb)->GetControl();

    EditorSelection selection;
    if (!GetSelectionInEditor(selection, control))
        return;

    ConfigManager *cfgEditor = Manager::Get()->GetConfigManager("editor");
    bool highlightOccurrences = cfgEditor->ReadBool("/highlight_occurrence/enabled", true);

    // Select the next occurrence first. This prevents a cursor created at the beginning of the
    // file when the user uses the command when there is a single selection. Scintilla always makes
    // sure that there is at least one selection/cursor. So if we clear all selections it creates a
    // cursor at the beginning of the file.
    const wxString &selectedText(control->GetTextRange(selection.GetStart(), selection.GetEnd()));
    if (!SelectNext(control, selectedText, selection.GetEnd(), selection.IsReversed()))
        return; // If there is no new selection don't deselect the current one.

    // store the selections in a vector except for the current one
    typedef std::vector<EditorSelection> Selections;
    Selections selections;
    int count = control->GetSelections();
    for (int ii = 0; ii < count; ++ii)
    {
        EditorSelection item;
        item.caret = control->GetSelectionNCaret(ii);
        item.anchor = control->GetSelectionNAnchor(ii);

        if (!item.Contains(selection))
            selections.push_back(item);
        else if (highlightOccurrences)
        {
            // Restore the indicator for the highlight occurrences if they are enabled.
            control->IndicatorFillRange(item.GetStart(), item.GetEnd());
        }
    }

    control->ClearSelections();
    Selections::const_iterator it = selections.begin();
    int index = 0;
    if (it != selections.end() && control->GetSelections() > 0)
    {
        control->SetSelectionNAnchor(index, it->anchor);
        control->SetSelectionNCaret(index, it->caret);
        ++index;
        ++it;
    }
    for (; it != selections.end(); ++it)
    {
        control->AddSelection(it->caret, it->anchor);
        ++index;
    }
}

/* This is a shameless rip-off of the original OnEditCommentSelected function,
 * now more suitingly named OnEditToggleCommentSelected (because that's what
 * it does :)
 */
void MainFrame::OnEditCommentSelected(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    CommentToken comment = colour_set->GetCommentToken( ed->GetLanguage() );
    if (comment.lineComment==wxEmptyString && comment.streamCommentStart==wxEmptyString)
        return;

    stc->BeginUndoAction();
    if ( wxSCI_INVALID_POSITION != stc->GetSelectionStart() )
    {
        int startLine = stc->LineFromPosition( stc->GetSelectionStart() );
        int endLine   = stc->LineFromPosition( stc->GetSelectionEnd() );
        int curLine=startLine;
        /**
            Fix a glitch: when selecting multiple lines and the caret
            is at the start of the line after the last line selected,
            the code would, wrongly, (un)comment that line too.
            This fixes it.
        */
        if (startLine != endLine && // selection is more than one line
            stc->GetColumn( stc->GetSelectionEnd() ) == 0) // and the caret is at the start of the line
        {
            // don't take into account the line the caret is on,
            // because it contains no selection (caret_column == 0)...
            --endLine;
        }

        while( curLine <= endLine )
        {
            // For each line: comment.
            if (comment.lineComment!=wxEmptyString)
                stc->InsertText( stc->PositionFromLine( curLine ), comment.lineComment );
            else // if the language doesn't support line comments use stream comments
            {
                stc->InsertText( stc->PositionFromLine( curLine ), comment.streamCommentStart );
                stc->InsertText( stc->GetLineEndPosition( curLine ), comment.streamCommentEnd );
            }
            ++curLine;
        } // end while
        stc->SetSelectionVoid(stc->PositionFromLine(startLine),stc->PositionFromLine(endLine)+stc->LineLength(endLine));
    }
    stc->EndUndoAction();
}

/* See above (OnEditCommentSelected) for details. */
void MainFrame::OnEditUncommentSelected(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    CommentToken comment = colour_set->GetCommentToken( ed->GetLanguage() );
    if (comment.lineComment==wxEmptyString && comment.streamCommentStart==wxEmptyString)
        return;

    stc->BeginUndoAction();
    if ( wxSCI_INVALID_POSITION != stc->GetSelectionStart() )
    {
        int startLine = stc->LineFromPosition( stc->GetSelectionStart() );
        int endLine   = stc->LineFromPosition( stc->GetSelectionEnd() );
        int curLine   = startLine;
        /**
            Fix a glitch: when selecting multiple lines and the caret
            is at the start of the line after the last line selected,
            the code would, wrongly, (un)comment that line too.
            This fixes it.
        */
        if (startLine != endLine && // selection is more than one line
        stc->GetColumn( stc->GetSelectionEnd() ) == 0) // and the caret is at the start of the line
        {
            // don't take into account the line the caret is on,
            // because it contains no selection (caret_column == 0)...
            --endLine;
        }

        while( curLine <= endLine )
        {
            // For each line: if it is commented, uncomment.
            wxString strLine = stc->GetLine( curLine );

            bool startsWithComment;
            bool endsWithComment;

            // check for line comment
            startsWithComment = strLine.Strip( wxString::leading ).StartsWith( comment.lineComment );
            if ( startsWithComment )
            {      // we know the comment is there (maybe preceded by white space)
                int Pos = strLine.Find(comment.lineComment);
                int start = stc->PositionFromLine( curLine ) + Pos;
                int end = start + comment.lineComment.length();
                stc->SetTargetStart( start );
                stc->SetTargetEnd( end );
                stc->ReplaceTarget( wxEmptyString );
            }

            // check for stream comment
            startsWithComment = strLine.Strip( wxString::leading  ).StartsWith( comment.streamCommentStart ); // check for stream comment start
            endsWithComment = strLine.Strip( wxString::trailing ).EndsWith( comment.streamCommentEnd); // check for stream comment end
            if ( startsWithComment && endsWithComment )
            {
                int Pos;
                int start;
                int end;

                // we know the start comment is there (maybe preceded by white space)
                Pos = strLine.Find(comment.streamCommentStart);
                start = stc->PositionFromLine( curLine ) + Pos;
                end = start + comment.streamCommentStart.length();
                stc->SetTargetStart( start );
                stc->SetTargetEnd( end );
                stc->ReplaceTarget( wxEmptyString );

                // we know the end comment is there too (maybe followed by white space)
                // attention!! we have to subtract the length of the comment we already removed
                Pos = strLine.rfind(comment.streamCommentEnd,strLine.npos) - comment.streamCommentStart.length();
                start = stc->PositionFromLine( curLine ) + Pos;
                end = start + comment.streamCommentEnd.length();
                stc->SetTargetStart( start );
                stc->SetTargetEnd( end );
                stc->ReplaceTarget( wxEmptyString );
            }
            ++curLine;
        } // end while
        stc->SetSelectionVoid(stc->PositionFromLine(startLine),stc->PositionFromLine(endLine)+stc->LineLength(endLine));
    }
    stc->EndUndoAction();
}

void MainFrame::OnEditToggleCommentSelected(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    wxString comment = colour_set->GetCommentToken( ed->GetLanguage() ).lineComment;
    if (comment.empty())
        return;

    stc->BeginUndoAction();
    if ( wxSCI_INVALID_POSITION != stc->GetSelectionStart() )
    {
        int startLine = stc->LineFromPosition( stc->GetSelectionStart() );
        int endLine   = stc->LineFromPosition( stc->GetSelectionEnd() );
        int curLine   = startLine;
        /**
            Fix a glitch: when selecting multiple lines and the caret
            is at the start of the line after the last line selected,
            the code would, wrongly, (un)comment that line too.
            This fixes it.
        */
        if (startLine != endLine && // selection is more than one line
        stc->GetColumn( stc->GetSelectionEnd() ) == 0) // and the caret is at the start of the line
        {
            // don't take into account the line the caret is on,
            // because it contains no selection (caret_column == 0)...
            --endLine;
        }

        bool doComment = false;
        while( curLine <= endLine )
        {
            // Check is any of the selected lines is commented
            wxString strLine = stc->GetLine( curLine );
            int commentPos = strLine.Strip( wxString::leading ).Find( comment );

            if (commentPos != 0)
            {
                // At least one line is not commented, so comment the whole selection
                // (else if all lines are commented, uncomment the selection)
                doComment = true;
                break;
            }
            ++curLine;
        }

        curLine = startLine;
        while( curLine <= endLine )
        {
            if (doComment)
                stc->InsertText( stc->PositionFromLine( curLine ), comment );
            else
            {
                // we know the comment is there (maybe preceded by white space)
                wxString strLine = stc->GetLine( curLine );
                int Pos = strLine.Find(comment);
                int start = stc->PositionFromLine( curLine ) + Pos;
                int end = start + comment.length();
                stc->SetTargetStart( start );
                stc->SetTargetEnd( end );
                stc->ReplaceTarget( wxEmptyString );
            }
            ++curLine;
        }
        stc->SetSelectionVoid(stc->PositionFromLine(startLine),stc->PositionFromLine(endLine)+stc->LineLength(endLine));
    }
    stc->EndUndoAction();
}

void MainFrame::OnEditStreamCommentSelected(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    CommentToken comment = colour_set->GetCommentToken( ed->GetLanguage() );
    if (comment.streamCommentStart.empty())
        return;

    stc->BeginUndoAction();
    if ( wxSCI_INVALID_POSITION != stc->GetSelectionStart() )
    {
        int startPos = stc->GetSelectionStart();
        int endPos   = stc->GetSelectionEnd();
        if ( startPos == endPos )
        {   // if nothing selected stream comment current *word* first
            startPos = stc->WordStartPosition(stc->GetCurrentPos(), true);
            endPos   = stc->WordEndPosition  (stc->GetCurrentPos(), true);
            if ( startPos == endPos )
            {   // if nothing selected stream comment current *line*
                startPos = stc->PositionFromLine  (stc->LineFromPosition(startPos));
                endPos   = stc->GetLineEndPosition(stc->LineFromPosition(startPos));
            }
        }
        else
        {
            /**
                Fix a glitch: when selecting multiple lines and the caret
                is at the start of the line after the last line selected,
                the code would, wrongly, (un)comment that line too.
                This fixes it.
            */
            if (stc->GetColumn( stc->GetSelectionEnd() ) == 0) // and the caret is at the start of the line
            {
                // don't take into account the line the caret is on,
                // because it contains no selection (caret_column == 0)...
                --endPos;
            }
        }
        // stream comment block
        int p1 = startPos - 1;
        while (stc->GetCharAt(p1) == ' ' && p1 > 0)
            --p1;
        p1 -= 1;
        int p2 = endPos;
        while (stc->GetCharAt(p2) == ' ' && p2 < stc->GetLength())
            ++p2;
        const wxString start = stc->GetTextRange(p1, p1 + comment.streamCommentStart.length());
        const wxString end = stc->GetTextRange(p2, p2 + comment.streamCommentEnd.length());
        if (start == comment.streamCommentStart && end == comment.streamCommentEnd)
        {
            stc->SetTargetStart(p1);
            stc->SetTargetEnd(p2 + 2);
            wxString target = stc->GetTextRange(p1 + 2, p2);
            stc->ReplaceTarget(target);
            stc->GotoPos(p1 + target.length());
        }
        else
        {
            stc->InsertText( startPos, comment.streamCommentStart );
            // we already inserted some characters so out endPos changed
            startPos += comment.streamCommentStart.length();
            endPos += comment.streamCommentStart.length();
            stc->InsertText( endPos, comment.streamCommentEnd );
            stc->SetSelectionVoid(startPos,endPos);
        }

    }
    stc->EndUndoAction();
}

void MainFrame::OnEditBoxCommentSelected(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    CommentToken comment = colour_set->GetCommentToken( ed->GetLanguage() );
    if (comment.boxCommentStart.empty())
        return;

    stc->BeginUndoAction();
    if ( wxSCI_INVALID_POSITION != stc->GetSelectionStart() )
    {
        int startLine = stc->LineFromPosition( stc->GetSelectionStart() );
        int endLine   = stc->LineFromPosition( stc->GetSelectionEnd() );
        int curLine   = startLine;
        /**
            Fix a glitch: when selecting multiple lines and the caret
            is at the start of the line after the last line selected,
            the code would, wrongly, (un)comment that line too.
            This fixes it.
        */
        if (startLine != endLine && // selection is more than one line
        stc->GetColumn( stc->GetSelectionEnd() ) == 0) // and the caret is at the start of the line
        {
            // don't take into account the line the caret is on,
            // because it contains no selection (caret_column == 0)...
            --endLine;
        }



        if (startLine == endLine) // if selection is only one line ...
        {
            // ... then insert streamcomment tokens at the beginning and the end of the line
            stc->InsertText( stc->PositionFromLine  ( curLine ), comment.streamCommentStart );
            stc->InsertText( stc->GetLineEndPosition( curLine ), comment.streamCommentEnd   );
        }
        else // selection is more than one line
        {
            // insert boxcomment start token
            stc->InsertText( stc->PositionFromLine( curLine ), comment.boxCommentStart );
            ++curLine; // we already commented the first line about 9 lines above
            while( curLine <= endLine )
            {
                // For each line: comment.
                stc->InsertText( stc->PositionFromLine( curLine ), comment.boxCommentMid );
                ++curLine;
            } // end while

            // insert boxcomment end token and add a new line character
            stc->InsertText( stc->PositionFromLine( curLine ), comment.boxCommentEnd + GetEOLStr(stc->GetEOLMode()) );
        } // end if
        stc->SetSelectionVoid(stc->PositionFromLine(startLine),stc->PositionFromLine(endLine)+stc->LineLength(endLine));
    }
    stc->EndUndoAction();
}

void MainFrame::OnEditShowCallTip(cb_unused wxCommandEvent& event)
{
    CodeBlocksEvent evt(cbEVT_SHOW_CALL_TIP);
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::OnEditCompleteCode(cb_unused wxCommandEvent& event)
{
    CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::OnEditHighlightMode(wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    HighlightLanguage lang = colour_set->GetHighlightLanguage("");
    if (event.GetId() != idEditHighlightModeText)
    {
        wxMenu* hl = nullptr;
        GetMenuBar()->FindItem(idEditHighlightModeText, &hl);
        if (hl)
        {
            wxMenuItem* item = hl->FindItem(event.GetId());
            if (item)
                lang = colour_set->GetHighlightLanguage(item->GetItemLabelText());
        }
    }

    // Just to update the text on the highlight button
    DoUpdateStatusBar();

    ed->SetLanguage(lang, true);
    Manager::Get()->GetCCManager()->NotifyPluginStatus();
}

void MainFrame::OnEditHighlightModeUpdateUI(wxUpdateUIEvent &event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    cbEditor *ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed == nullptr)
    {
        event.Enable(false);
        return;
    }
    EditorColourSet* colour_set = ed->GetColourSet();
    if (colour_set == nullptr)
    {
        event.Enable(false);
        return;
    }

    const wxString &languageName = colour_set->GetLanguageName(ed->GetLanguage());

    const int id = event.GetId();
    MenuIDToLanguage::const_iterator it = m_MapMenuIDToLanguage.find(id);
    if (it != m_MapMenuIDToLanguage.end())
        event.Check(languageName == it->second);
    else
    {
        if (id == idEditHighlightModeText)
            event.Check(languageName == "Plain text");
        else
        {
            // Unknown language, just disable.
            event.Enable(false);
            return;
        }
    }
    event.Enable(true);
}

void MainFrame::OnEditFoldAll(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->FoldAll();
}

void MainFrame::OnEditUnfoldAll(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->UnfoldAll();
}

void MainFrame::OnEditToggleAllFolds(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->ToggleAllFolds();
}

void MainFrame::OnEditFoldBlock(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->FoldBlockFromLine();
}

void MainFrame::OnEditUnfoldBlock(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->UnfoldBlockFromLine();
}

void MainFrame::OnEditToggleFoldBlock(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->ToggleFoldBlockFromLine();
}

void MainFrame::OnEditEOLMode(wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        int mode = -1;

        if (event.GetId() == idEditEOLCRLF)
            mode = wxSCI_EOL_CRLF;
        else if (event.GetId() == idEditEOLCR)
            mode = wxSCI_EOL_CR;
        else if (event.GetId() == idEditEOLLF)
            mode = wxSCI_EOL_LF;

        if (mode != -1 && mode != ed->GetControl()->GetEOLMode())
        {
            ed->GetControl()->BeginUndoAction();
            ed->GetControl()->ConvertEOLs(mode);
            ed->GetControl()->SetEOLMode(mode);
            ed->GetControl()->EndUndoAction();
        }
    }
}

void MainFrame::OnEditEncoding(wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    if ( event.GetId() == idEditEncodingUseBom )
    {
        ed->SetUseBom( !ed->GetUseBom() );
        return;
    }

    wxFontEncoding encoding = wxFONTENCODING_SYSTEM;

    if ( event.GetId() == idEditEncodingDefault )
        encoding = wxFONTENCODING_SYSTEM;
    else if ( event.GetId() == idEditEncodingAscii )
        encoding = wxFONTENCODING_ISO8859_1;
    else if ( event.GetId() == idEditEncodingUtf7 )
        encoding = wxFONTENCODING_UTF7;
    else if ( event.GetId() == idEditEncodingUtf8 )
        encoding = wxFONTENCODING_UTF8;
    else if ( event.GetId() == idEditEncodingUtf16 )
        encoding = wxFONTENCODING_UTF16;
    else if ( event.GetId() == idEditEncodingUtf32 )
        encoding = wxFONTENCODING_UTF32;
    else if ( event.GetId() == idEditEncodingUnicode )
        encoding = wxFONTENCODING_UNICODE;
    else if ( event.GetId() == idEditEncodingUnicode16BE )
        encoding = wxFONTENCODING_UTF16BE;
    else if ( event.GetId() == idEditEncodingUnicode16LE )
        encoding = wxFONTENCODING_UTF16LE;
    else if ( event.GetId() == idEditEncodingUnicode32BE )
        encoding = wxFONTENCODING_UTF32BE;
    else if ( event.GetId() == idEditEncodingUnicode32LE )
        encoding = wxFONTENCODING_UTF32LE;

    ed->SetEncoding(encoding);
}

void MainFrame::OnViewLayout(wxCommandEvent& event)
{
    LoadViewLayout(m_PluginIDsMap[event.GetId()]);
}

void MainFrame::OnViewLayoutSave(cb_unused wxCommandEvent& event)
{
    wxString def = m_LastLayoutName;
    if ( def.empty() )
        def = Manager::Get()->GetConfigManager("app")->Read("/main_frame/layout/default");

    const wxString name(cbGetTextFromUser(_("Enter the name for this perspective"),
                                          _("Save current perspective"), def, this));
    if (!name.empty())
    {
        DoFixToolbarsLayout();
        SaveViewLayout(name,
                       m_LayoutManager.SavePerspective(),
                       m_pInfoPane->SaveTabOrder(),
                       true);
    }
}

void MainFrame::OnViewLayoutDelete(cb_unused wxCommandEvent& event)
{
    if (m_LastLayoutName == gDefaultLayout)
    {
        if (cbMessageBox(_("The default perspective cannot be deleted. It can always be reverted to "
                        "a predefined state though.\nDo you want to revert it now?"),
                        _("Confirmation"),
                        wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT) == wxID_YES)
        {
            m_LayoutViews[gDefaultLayout] = gDefaultLayoutData;
            m_LayoutMessagePane[gDefaultLayout] = gDefaultMessagePaneLayoutData;
            LoadViewLayout(gDefaultLayout);
        }
        return;
    }

    if (m_LastLayoutName == gMinimalLayout)
    {
        if (cbMessageBox(_("The minimal layout cannot be deleted. It can always be reverted to "
                        "a predefined state though.\nDo you want to revert it now?"),
                        _("Confirmation"),
                        wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT) == wxID_YES)
        {
            wxString tempLayout = m_PreviousLayoutName;
            m_LayoutViews[gMinimalLayout] = gMinimalLayoutData;
            m_LayoutMessagePane[gMinimalLayout] = gMinimalMessagePaneLayoutData;
            LoadViewLayout(gMinimalLayout);
            m_PreviousLayoutName = tempLayout;
        }
        return;
    }

    if (cbMessageBox(wxString::Format(_("Are you really sure you want to delete the perspective '%s'?"), m_LastLayoutName),
                    _("Confirmation"),
                    wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT) == wxID_YES)
    {
        // first delete it from the hashmap
        LayoutViewsMap::iterator it = m_LayoutViews.find(m_LastLayoutName);
        if (it != m_LayoutViews.end())
            m_LayoutViews.erase(it);
        it = m_LayoutMessagePane.find(m_LastLayoutName);
        if (it != m_LayoutMessagePane.end())
            m_LayoutMessagePane.erase(it);

        // now delete the menu item too
        wxMenu* viewLayouts = nullptr;
        GetMenuBar()->FindItem(idViewLayoutSave, &viewLayouts);
        if (viewLayouts)
        {
            int id = viewLayouts->FindItem(m_LastLayoutName);
            if (id != wxNOT_FOUND)
                viewLayouts->Delete(id);
            // delete the id from the map too
            PluginIDsMap::iterator it2 = m_PluginIDsMap.find(id);
            if (it2 != m_PluginIDsMap.end())
                m_PluginIDsMap.erase(it2);
        }

        cbMessageBox(wxString::Format(_("Perspective '%s' deleted.\nWill now revert to perspective '%s'..."), m_LastLayoutName, gDefaultLayout),
                        _("Information"), wxICON_INFORMATION);

        // finally, revert to the default layout
        m_LastLayoutName = gDefaultLayout; // do not ask to save old layout ;)
        LoadViewLayout(gDefaultLayout);
    }
}

void MainFrame::OnNotebookDoubleClick(cb_unused CodeBlocksEvent& event)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    if (m_LastLayoutName == gMinimalLayout)
        LoadViewLayout(m_PreviousLayoutName.empty() ? cfg->Read("/environment/view/layout_to_toggle", gDefaultLayout) : m_PreviousLayoutName);
    else
    {
        if (cfg->ReadBool("/environment/view/dbl_clk_maximize", true))
            LoadViewLayout(gMinimalLayout);
    }
}

void MainFrame::OnViewScriptConsole(cb_unused wxCommandEvent& event)
{
    ShowHideScriptConsole();
}

void MainFrame::OnViewHideEditorTabs(cb_unused wxCommandEvent& event)
{
	cbAuiNotebook* nb = Manager::Get()->GetEditorManager()->GetNotebook();
	if (nb)
	{
		bool hide_editor_tabs = nb->GetTabCtrlHeight() > 0;

		if (hide_editor_tabs)
			nb->SetTabCtrlHeight(0);
		else
			nb->SetTabCtrlHeight(-1);

		Manager::Get()->GetConfigManager("app")->Write("/environment/hide_editor_tabs", hide_editor_tabs);
	}
}

void MainFrame::OnSearchFind(wxCommandEvent& event)
{
    bool bDoMultipleFiles = (event.GetId() == idSearchFindInFiles);
    if (!bDoMultipleFiles)
        bDoMultipleFiles = !Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    m_findReplace.ShowFindDialog(false, bDoMultipleFiles);
}

void MainFrame::OnSearchFindNext(wxCommandEvent& event)
{
    bool bNext = !(event.GetId() == idSearchFindPrevious);
    m_findReplace.FindNext(bNext, nullptr, nullptr, false);
}

void MainFrame::OnSearchFindNextSelected(wxCommandEvent& event)
{
    bool bNext = !(event.GetId() == idSearchFindSelectedPrevious);
    m_findReplace.FindSelectedText(bNext);
}

void MainFrame::OnSearchReplace(wxCommandEvent& event)
{
    bool bDoMultipleFiles = (event.GetId() == idSearchReplaceInFiles);
    if (!bDoMultipleFiles)
        bDoMultipleFiles = !Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    m_findReplace.ShowFindDialog(true, bDoMultipleFiles);
}

void MainFrame::OnSearchGotoLine(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    int max = ed->GetControl()->LineFromPosition(ed->GetControl()->GetLength()) + 1;

    /**
    @remarks We use wxGetText instead of wxGetNumber because wxGetNumber *must*
    provide an initial line number...which doesn't make sense, and just keeps the
    user deleting the initial line number everytime he instantiates the dialog.
    However, this is just a temporary hack, because the default dialog used isn't
    that suitable either.
    */
    wxString strLine = cbGetTextFromUser( wxString::Format(_("Line (1 - %d): "), max),
                                        _("Goto line"),
                                        "",
                                        this );
    long int line = 0;
    strLine.ToLong(&line);
    if ( line >= 1 && line <= max )
    {
        ed->UnfoldBlockFromLine(line - 1);
        ed->GotoLine(line - 1);
    }
}

void MainFrame::OnSearchGotoNextChanged(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GotoNextChanged();
}

void MainFrame::OnSearchGotoPrevChanged(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->GotoPreviousChanged();
}

void MainFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event))
{
    dlgAbout dlg(this);
    PlaceWindow(&dlg, pdlHead);
    dlg.ShowModal();
}

void MainFrame::OnHelpTips(cb_unused wxCommandEvent& event)
{
    ShowTips(true);
}

void MainFrame::OnFileMenuUpdateUI(wxUpdateUIEvent& event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Skip();
        return;
    }

    if (!ProjectManager::CanShutdown() || !EditorManager::CanShutdown())
    {
        event.Enable(false);
        return;
    }


    const int id = event.GetId();

    // Single file related menu items
    if (id == idFileClose || id == idFileCloseAll || id == idFileSaveAs)
    {
        EditorManager *editorManager = Manager::Get()->GetEditorManager();
        EditorBase *ed = (editorManager ? editorManager->GetActiveEditor() : nullptr);
        if (ed == nullptr)
            event.Enable(false);
        else if (ed->IsBuiltinEditor())
            event.Enable(true);
        else
        {
            // Detect if this is the start here page.
            event.Enable(ed->GetTitle() != GetStartHereTitle());
        }
    }
    else if (id == idFileSave)
    {
        EditorManager *editorManager = Manager::Get()->GetEditorManager();
        EditorBase *ed = (editorManager ? editorManager->GetActiveEditor() : nullptr);
        event.Enable(ed && ed->GetModified());
    }
    else if (id == idFilePrint)
    {
        EditorManager *editorManager = Manager::Get()->GetEditorManager();
        event.Enable(editorManager && editorManager->GetBuiltinActiveEditor());
    }
    else if (id == idFileOpen)
        event.Enable(true);
    else
    {
        ProjectManager *projectManager = Manager::Get()->GetProjectManager();
        cbProject *project = (projectManager ? projectManager->GetActiveProject() : nullptr);
        if (project && project->GetCurrentlyCompilingTarget())
        {
            event.Enable(false);
            return;
        }

        // Project related menu items
        if (id == idFileReopenProject)
            event.Enable(true);
        else if (id == idFileCloseProject || id == idFileSaveProjectAs || id == idFileSaveProjectTemplate)
            event.Enable(project != nullptr);
        else if (id == idFileSaveProject)
            event.Enable(project && project->GetModified());
        else if (id == idFileOpenDefWorkspace || id == idFileSaveWorkspaceAs || id == idFileSaveAll)
            event.Enable(true);
        else
        {
            // Workspace related menu items
            const cbWorkspace *workspace = Manager::Get()->GetProjectManager()->GetWorkspace();

            if (id == idFileSaveWorkspace)
                event.Enable(workspace && workspace->GetModified());
            else if (id == idFileCloseWorkspace)
                event.Enable(workspace != nullptr);
            else
                event.Skip();
        }
    }
}

static void SetupEOLItem(wxUpdateUIEvent &event, EditorBase *editor, int targetEOLMode)
{
    if (!editor->IsBuiltinEditor())
        event.Enable(false);
    else
    {
        event.Enable(true);
        const int eolMode = static_cast<cbEditor*>(editor)->GetControl()->GetEOLMode();
        event.Check(eolMode == targetEOLMode);
    }
}

void MainFrame::OnEditMenuUpdateUI(wxUpdateUIEvent& event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    const int id = event.GetId();

    EditorManager *editorManager = Manager::Get()->GetEditorManager();
    EditorBase *eb = editorManager->GetActiveEditor();
    if (!eb)
    {
        event.Enable(false);
        return;
    }

    if (id == idEditUndo)
        event.Enable(eb->CanUndo());
    else if (id == idEditRedo)
        event.Enable(eb->CanRedo());
    else if (id == idEditClearHistory)
        event.Enable(eb->CanUndo() || eb->CanRedo());
    else if (id == idEditCut)
        event.Enable(!eb->IsReadOnly() && eb->HasSelection());
    else if (id == idEditCopy || id == idEditSelectNextSkip)
        event.Enable(eb->HasSelection());
    else if (id == idEditPaste)
        event.Enable(eb->CanPaste());
    else if (id == idEditSwapHeaderSource || id == idEditGotoMatchingBrace
             || id == idEditHighlightMode || id == idEditSelectNext || id == idEditBookmarks
             || id == idEditEOLMode || id == idEditEncoding || id == idEditSpecialCommands
             || id == idEditCommentSelected || id == idEditUncommentSelected
             || id == idEditToggleCommentSelected || id == idEditStreamCommentSelected
             || id == idEditBoxCommentSelected || id == idEditShowCallTip
             || id == idEditCompleteCode)
    {
        event.Enable(eb->IsBuiltinEditor());
    }
    else if (id == idEditSelectAll)
        event.Enable(eb->CanSelectAll());
    else if (id == idEditFolding)
    {
        if (eb->IsBuiltinEditor())
        {
            bool showFolds = Manager::Get()->GetConfigManager("editor")->ReadBool("/folding/show_folds", false);
            event.Enable(showFolds);
        }
        else
            event.Enable(false);
    }
    else if (id == idEditSpecialCommandsCase)
        event.Enable(eb->IsBuiltinEditor() && eb->HasSelection());
    else if (id == idEditEOLCRLF)
        SetupEOLItem(event, eb, wxSCI_EOL_CRLF);
    else if (id == idEditEOLCR)
        SetupEOLItem(event, eb, wxSCI_EOL_CR);
    else if (id == idEditEOLLF)
        SetupEOLItem(event, eb, wxSCI_EOL_LF);
    else
    {
        cbEditor *ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (!ed)
        {
            event.Enable(false);
            return;
        }
        event.Enable(true);

        if (id == idEditEncodingDefault)
        {
            const wxFontEncoding encoding = ed->GetEncoding();
            event.Check(encoding == wxFONTENCODING_SYSTEM || encoding == wxLocale::GetSystemEncoding());
        }
        else if (id == idEditEncodingUseBom)
            event.Check(ed->GetUseBom());
        else if (id == idEditEncodingAscii)
            event.Check(ed->GetEncoding() == wxFONTENCODING_ISO8859_1);
        else if (id == idEditEncodingUtf7)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF7);
        else if (id == idEditEncodingUtf8)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF8);
        else if (id == idEditEncodingUnicode)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UNICODE);
        else if (id == idEditEncodingUtf16)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF16);
        else if (id == idEditEncodingUtf32)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF32);
        else if (id == idEditEncodingUnicode16BE)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF16BE);
        else if (id == idEditEncodingUnicode16LE)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF16LE);
        else if (id == idEditEncodingUnicode32BE)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF32BE);
        else if (id == idEditEncodingUnicode32LE)
            event.Check(ed->GetEncoding() == wxFONTENCODING_UTF32LE);
    }
}

void MainFrame::OnViewMenuUpdateUI(wxUpdateUIEvent& event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    const int id = event.GetId();
    if (id == idViewManager)
    {
        const bool managerVisibility = m_LayoutManager.GetPane(m_pPrjManUI->GetNotebook()).IsShown();
        event.Check(managerVisibility);
    }
    else if (id == idViewFocusManagement)
    {
        const bool managerVisibility = m_LayoutManager.GetPane(m_pPrjManUI->GetNotebook()).IsShown();
        event.Enable(managerVisibility);
    }
    else if (id == idViewLogManager)
        event.Check(m_LayoutManager.GetPane(m_pInfoPane).IsShown());
    else if (id == idViewStartPage)
    {
        EditorManager *editorManager = Manager::Get()->GetEditorManager();
        const int editorCount = editorManager->GetEditorsCount();
        bool found = false;
        for (int ii = 0; ii < editorCount; ++ii)
        {
            EditorBase *editor = editorManager->GetEditor(ii);
            if (editor && editor->GetTitle() == GetStartHereTitle())
            {
                found = true;
                break;
            }
        }

        event.Check(found);
    }
    else if (id == idViewStatusbar)
    {
        wxStatusBar *statusBar = GetStatusBar();
        event.Check(statusBar && statusBar->IsShown());
    }
    else if (id == idViewScriptConsole)
        event.Check(m_LayoutManager.GetPane(m_pScriptConsole).IsShown());
    else if (id == idViewHideEditorTabs)
        event.Check(Manager::Get()->GetEditorManager()->GetNotebook()->GetTabCtrlHeight() == 0);
    else if (id == idViewFullScreen)
        event.Check(IsFullScreen());
    else if (id == idViewFocusEditor)
    {
        EditorManager *editorManager = Manager::Get()->GetEditorManager();
        event.Enable(editorManager && editorManager->GetBuiltinActiveEditor() != nullptr);
    }
    else if (id == idViewFocusLogsAndOthers)
        event.Enable(m_pInfoPane->IsShown());
    else if (id == idViewToolMain)
        event.Check(m_LayoutManager.GetPane(m_pToolbar).IsShown());
    else if (id == idViewToolDebugger)
        event.Check(m_LayoutManager.GetPane(m_debuggerToolbarHandler->GetToolbar(false)).IsShown());
}

void MainFrame::OnUpdateCheckablePluginMenu(wxUpdateUIEvent &event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    bool check = false;

    const int id = event.GetId();
    PluginIDsMap::const_iterator it = m_PluginIDsMap.find(id);
    if (it != m_PluginIDsMap.end())
    {
        const wxString &pluginName = it->second;
        if (!pluginName.empty())
        {
            cbPlugin* plugin = Manager::Get()->GetPluginManager()->FindPluginByName(pluginName);
            if (plugin)
                check = m_LayoutManager.GetPane(m_PluginsTools[plugin]).IsShown();
        }
    }

    event.Check(check);
}

void MainFrame::OnSearchMenuUpdateUI(wxUpdateUIEvent& event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    const int id = event.GetId();
    if (id == idSearchFindInFiles || id == idSearchReplaceInFiles)
    {
        // 'Find' and 'Replace' are always enabled for (find|replace)-in-files
        event.Enable(true);
        return;
    }

    EditorManager *editorManager = Manager::Get()->GetEditorManager();
    if (editorManager == nullptr)
    {
        event.Enable(false);
        return;
    }
    cbEditor* ed = editorManager->GetBuiltinActiveEditor();
    if (ed == nullptr)
    {
        event.Enable(false);
        return;
    }

    if (id == idSearchGotoNextChanged || id == idSearchGotoPreviousChanged)
    {
        bool useChangeBar = Manager::Get()->GetConfigManager("editor")->ReadBool("/margin/use_changebar", true);
        event.Enable(useChangeBar && (ed->CanUndo() || ed->CanRedo()));
    }
    else
        event.Enable(true);
}


void MainFrame::OnEditorUpdateUI(CodeBlocksEvent& event)
{
    if (Manager::IsAppShuttingDown())
    {
        event.Skip();
        return;
    }

    if (Manager::Get()->GetEditorManager() && event.GetEditor() == Manager::Get()->GetEditorManager()->GetActiveEditor())
    {
        // Execute the code to update the status bar outside of the paint event for scintilla.
        // Executing this function directly in the event handler causes redraw problems on Windows.
        CallAfter(&MainFrame::DoUpdateStatusBar);
    }

    event.Skip();
}

void MainFrame::OnViewToolbarsFit(cb_unused wxCommandEvent& event)
{
    FitToolbars(m_LayoutManager, this);
    DoUpdateLayout();
}

void MainFrame::OnViewToolbarsOptimize(cb_unused wxCommandEvent& event)
{
    OptimizeToolbars(m_LayoutManager, this);
    DoUpdateLayout();
}

void MainFrame::OnToggleBar(wxCommandEvent& event)
{
    wxWindow* win = nullptr;
    bool toolbar = false;
    if (event.GetId() == idViewManager)
        win = m_pPrjManUI->GetNotebook();
    else if (event.GetId() == idViewLogManager)
        win = m_pInfoPane;
    else if (event.GetId() == idViewToolMain)
    {
        win = m_pToolbar;
        toolbar = true;
    }
    else if (event.GetId() == idViewToolDebugger)
    {
        win = m_debuggerToolbarHandler->GetToolbar();
        toolbar = true;
    }
    else
    {
        const wxString pluginName(m_PluginIDsMap[event.GetId()]);
        if (!pluginName.empty())
        {
            cbPlugin* plugin = Manager::Get()->GetPluginManager()->FindPluginByName(pluginName);
            if (plugin)
            {
                win = m_PluginsTools[plugin];
                toolbar = true;
            }
        }
    }

    if (win)
    {
        // For checked menu items, event.IsChecked() will not reflect the actual status of the menu item
        //  when this event was previously event.Skip()'ed after the menu item change.
        bool isShown = m_LayoutManager.GetPane(win).IsShown();

        // use last visible size as BestSize, Logs & others does no longer "forget" it's size
        if (!isShown)
             m_LayoutManager.GetPane(win).BestSize(win->GetSize());

        m_LayoutManager.GetPane(win).Show(not isShown); //toggle
        if (toolbar)
            FitToolbars(m_LayoutManager, this);
        DoUpdateLayout();
    }
}

void MainFrame::OnToggleStatusBar(cb_unused wxCommandEvent& event)
{
    cbStatusBar* sb = (cbStatusBar*)GetStatusBar();
    if (!sb) return;

    ConfigManager* cfg = Manager::Get()->GetConfigManager("app");
    const bool show = !cfg->ReadBool("/main_frame/statusbar", true);
    cfg->Write("/main_frame/statusbar", show);

    DoUpdateStatusBar();
    sb->Show(show);
    if ( show ) SendSizeEvent();
    DoUpdateLayout();
}

void MainFrame::OnFocusEditor(cb_unused wxCommandEvent& event)
{
    EditorManager* edman = Manager::Get()->GetEditorManager();
    cbAuiNotebook* nb = edman?edman->GetNotebook():nullptr;
    if (nb)
        nb->FocusActiveTabCtrl();
}

void MainFrame::OnFocusManagement(cb_unused wxCommandEvent& event)
{
    cbAuiNotebook* nb = m_pPrjManUI ? m_pPrjManUI->GetNotebook() : nullptr;
    if (nb)
        nb->FocusActiveTabCtrl();
}

void MainFrame::OnFocusLogsAndOthers(cb_unused wxCommandEvent& event)
{
    if (m_pInfoPane)
        m_pInfoPane->FocusActiveTabCtrl();
}

void MainFrame::OnSwitchTabs(cb_unused wxCommandEvent& event)
{
    // Get the notebook from the editormanager:
    cbAuiNotebook* nb = Manager::Get()->GetEditorManager()->GetNotebook();
    if (!nb)
        return;

    // Create container and add all open editors:
    wxSwitcherItems items;
    items.AddGroup(_("Open files"), "editors");
    if (!Manager::Get()->GetConfigManager("app")->ReadBool("/environment/tabs_stacked_based_switching"))
    {   // Switch tabs editor with tab order
        for (size_t i = 0; i < nb->GetPageCount(); ++i)
        {
            wxString title = nb->GetPageText(i);
            wxWindow* window = nb->GetPage(i);

            items.AddItem(title, title, GetEditorDescription(static_cast<EditorBase*> (window)), i, nb->GetPageBitmap(i)).SetWindow(window);
        }

        // Select the focused editor:
        int idx = items.GetIndexForFocus();
        if (idx != wxNOT_FOUND)
            items.SetSelection(idx);
    }
    else
    {   // Switch tabs editor with last used order
        int index = 0;
        cbNotebookStack* body;
        for (body = Manager::Get()->GetEditorManager()->GetNotebookStack(); body != nullptr; body = body->next)
        {
            index = nb->GetPageIndex(body->window);
            if (index == wxNOT_FOUND)
                continue;
            wxString title = nb->GetPageText(index);
            items.AddItem(title, title, GetEditorDescription(static_cast<EditorBase*> (body->window)), index, nb->GetPageBitmap(index)).SetWindow(body->window);
        }

        // Select the focused editor:
        if (items.GetItemCount() > 2)
            items.SetSelection(2); // CTRL + TAB directly select the last editor, not the current one
        else
            items.SetSelection(items.GetItemCount()-1);
    }

    // Create the switcher dialog
    wxSwitcherDialog dlg(items, wxGetApp().GetTopWindow());

    // Ctrl+Tab workaround for non windows platforms:
    if      (platform::cocoa)
        dlg.SetModifierKey(WXK_ALT);
    else if (platform::gtk)
        dlg.SetExtraNavigationKey(',');

    // Finally show the dialog:
    int answer = dlg.ShowModal();

    // If necessary change the selected editor:
    if ((answer == wxID_OK) && (dlg.GetSelection() != -1))
    {
        wxSwitcherItem& item = items.GetItem(dlg.GetSelection());
        wxWindow* win = item.GetWindow();
        if (win)
        {
            nb->SetSelection(item.GetId());
            win->SetFocus();
        }
    }
}

void MainFrame::OnToggleStartPage(cb_unused wxCommandEvent& event)
{
    int toggle = -1;
    if (Manager::Get()->GetEditorManager()->GetEditor(GetStartHereTitle()) == nullptr)
        toggle = 1;

    ShowHideStartPage(false, toggle);
}

void MainFrame::OnToggleFullScreen(cb_unused wxCommandEvent& event)
{
    ShowFullScreen( !IsFullScreen(), wxFULLSCREEN_NOTOOLBAR// | wxFULLSCREEN_NOSTATUSBAR
                    | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION );

    // Create full screen-close button if we're in full screen
    if ( IsFullScreen() )
    {
        //
        // Show the button to the bottom-right of the container
        //
        wxSize containerSize = GetClientSize();
        wxSize buttonSize = m_pCloseFullScreenBtn->GetSize();

        // Align
        m_pCloseFullScreenBtn->Move( containerSize.GetWidth() - buttonSize.GetWidth(),
                    containerSize.GetHeight() - buttonSize.GetHeight() );

        m_pCloseFullScreenBtn->Show( true );
        m_pCloseFullScreenBtn->Raise();
    }
    else
        m_pCloseFullScreenBtn->Show( false );
}

void MainFrame::OnPluginInstalled(CodeBlocksEvent& event)
{
    PluginsUpdated(event.GetPlugin(), Installed);
}

void MainFrame::OnPluginUninstalled(CodeBlocksEvent& event)
{
    if (Manager::IsAppShuttingDown())
        return;
    PluginsUpdated(event.GetPlugin(), Uninstalled);
}

void MainFrame::OnPluginLoaded(CodeBlocksEvent& event)
{
    cbPlugin* plug = event.GetPlugin();
    if (plug)
    {
        DoAddPlugin(plug);
        const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(plug);
        const wxString msg(info ? info->title : wxString(_("<Unknown plugin>")));
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("%s plugin activated", msg));
        OnViewToolbarsOptimize(event); // event is unused!
    }
}

void MainFrame::OnPluginUnloaded(CodeBlocksEvent& event)
{
    if (Manager::IsAppShuttingDown())
        return;

    cbPlugin* plugin = event.GetPlugin();

    cbStatusBar *sb = (cbStatusBar*)GetStatusBar();
    if (sb)
    {
        sb->RemoveField(plugin);
        sb->AdjustFieldsSize();
    }

    // remove toolbar, if any
    if (m_PluginsTools[plugin])
    {
        // Disconnect the mouse right click event handler before the toolbar is destroyed
        bool result = m_PluginsTools[plugin]->Disconnect(wxID_ANY, wxEVT_COMMAND_TOOL_RCLICKED, wxCommandEventHandler(MainFrame::OnToolBarRightClick));
        cbAssert(result);
        m_LayoutManager.DetachPane(m_PluginsTools[plugin]);
        m_PluginsTools[plugin]->Destroy();
        m_PluginsTools.erase(plugin);
        DoUpdateLayout();
    }

    PluginsUpdated(plugin, Unloaded);
}

void MainFrame::OnSettingsEnvironment(cb_unused wxCommandEvent& event)
{
    bool needRestart = false;
    const int originalToolbarSize = cbHelpers::ReadToolbarSizeFromConfig();

    EnvironmentSettingsDlg dlg(this, m_LayoutManager.GetArtProvider());
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        DoUpdateEditorStyle();
        DoUpdateLayoutColours();

        {
            ConfigManager *cfg = Manager::Get()->GetConfigManager("app");
            const int newToolbarSize = cfg->ReadInt("/environment/toolbar_size", cbHelpers::defaultToolbarSize);
            needRestart = (newToolbarSize != originalToolbarSize);
        }

        Manager::Get()->GetLogManager()->NotifyUpdate();
        Manager::Get()->GetEditorManager()->RecreateOpenEditorStyles();
        Manager::Get()->GetCCManager()->UpdateEnvSettings();
        m_pPrjManUI->RebuildTree();
        ShowHideStartPage();

        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Environment));
        Manager::Get()->ProcessEvent(event2);
    }

    if (needRestart)
        cbMessageBox(_("Code::Blocks needs to be restarted for the changes to take effect."), _("Information"), wxICON_INFORMATION);
}

void MainFrame::OnGlobalUserVars(cb_unused wxCommandEvent& event)
{
    Manager::Get()->GetUserVariableManager()->Configure();
}

void MainFrame::OnBackticks(cb_unused wxCommandEvent& event)
{
    wxDialog dialog(this, wxID_ANY, _("Backtick Cache"), wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX);
    wxBoxSizer *mainSizer;
    mainSizer = new wxBoxSizer(wxVERTICAL);

    // Add list
    wxListCtrl *list = new wxListCtrl(&dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxLC_REPORT | wxVSCROLL | wxHSCROLL | wxLC_HRULES | wxLC_VRULES);
    list->SetMinSize(wxSize(600, 400));
    mainSizer->Add(list, 1, wxALL | wxEXPAND, 8);

    // Add Buttons
    wxStdDialogButtonSizer *btnSizer = new wxStdDialogButtonSizer();
    auto endModalHandler = [&dialog](wxCommandEvent &evt)
    {
        dialog.EndModal(evt.GetId());
        evt.Skip();
    };

    wxButton *clearButton = new wxButton(&dialog, wxID_APPLY, wxString(_("C&lear and close")));
    clearButton->Bind(wxEVT_BUTTON, endModalHandler);
    btnSizer->AddButton(clearButton);
    wxButton *closeButton = new wxButton(&dialog, wxID_CLOSE, wxString());
    closeButton->Bind(wxEVT_BUTTON, endModalHandler);
    btnSizer->AddButton(closeButton);
    btnSizer->Realize();
    mainSizer->Add(btnSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

    // Size
    dialog.SetSizer(mainSizer);
    mainSizer->SetSizeHints(&dialog);

    list->InsertColumn(0, _("Expression"), wxLIST_FORMAT_LEFT, 100);
    list->InsertColumn(1, _("Value"), wxLIST_FORMAT_LEFT, 300);

    const cbBackticksMap &map = cbGetBackticksCache();
    for (const cbBackticksMap::value_type &item : map)
    {
        const int count = list->GetItemCount();
        list->InsertItem(count, item.first);
        list->SetItem(count, 1, item.second);
    }

    if (list->GetItemCount() > 0)
    {
        list->SetColumnWidth(0, wxLIST_AUTOSIZE);
        list->SetColumnWidth(1, wxLIST_AUTOSIZE);
    }

    PlaceWindow(&dialog);
    if (dialog.ShowModal() == wxID_APPLY)
        cbClearBackticksCache();
}

void MainFrame::OnSettingsEditor(cb_unused wxCommandEvent& event)
{
    // editor lexers loading takes some time; better reflect this with a hourglass
    wxBeginBusyCursor();

    EditorConfigurationDlg dlg(Manager::Get()->GetAppWindow());
    PlaceWindow(&dlg);

    // done, restore pointer
    wxEndBusyCursor();

    if (dlg.ShowModal() == wxID_OK)
    {
        Manager::Get()->GetEditorManager()->RecreateOpenEditorStyles();

        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Editor));
        Manager::Get()->ProcessEvent(event2);
    }
}

void MainFrame::OnSettingsCompiler(cb_unused wxCommandEvent& event)
{
    CompilerSettingsDlg dlg(this);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Compiler));
        Manager::Get()->ProcessEvent(event2);
    }
}

void MainFrame::OnSettingsDebugger(cb_unused wxCommandEvent& event)
{
    DebuggerSettingsDlg dlg(this);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Debugger));
        Manager::Get()->ProcessEvent(event2);
    }
}

void MainFrame::OnSettingsPlugins(cb_unused wxCommandEvent& event)
{
    if (Manager::Get()->GetPluginManager()->Configure() == wxID_OK)
    {
        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Plugins));
        Manager::Get()->ProcessEvent(event2);
    }
}

void MainFrame::OnSettingsScripting(cb_unused wxCommandEvent& event)
{
    ScriptingSettingsDlg dlg(this);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        RunStartupScripts();

        CodeBlocksEvent event2(cbEVT_SETTINGS_CHANGED);
        event2.SetInt(int(cbSettingsType::Scripting));
        Manager::Get()->ProcessEvent(event2);
    }
}

void MainFrame::OnProjectActivated(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    event.Skip();
}

void MainFrame::OnProjectOpened(CodeBlocksEvent& event)
{
    ShowHideStartPage(true);
    event.Skip();
}

void MainFrame::OnEditorOpened(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    event.Skip();
}

void MainFrame::OnEditorActivated(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    DoUpdateStatusBar();

    EditorBase *editor = event.GetEditor();
    if (editor && editor->IsBuiltinEditor())
    {
        ConfigManager* cfgEditor = Manager::Get()->GetConfigManager("editor");
        if (cfgEditor->ReadBool("/sync_editor_with_project_manager", false))
        {
            ProjectFile* pf = static_cast<cbEditor*>(editor)->GetProjectFile();
            if (pf)
                m_pPrjManUI->ShowFileInTree(*pf);
        }
    }

    event.Skip();
}

void MainFrame::OnEditorClosed(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    DoUpdateStatusBar();
    event.Skip();
}

void MainFrame::OnEditorSaved(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    event.Skip();
}

void MainFrame::OnEditorModified(CodeBlocksEvent& event)
{
    DoUpdateAppTitle();
    event.Skip();
}

void MainFrame::OnProjectClosed(CodeBlocksEvent& event)
{
    ShowHideStartPage();
    event.Skip();
}

void MainFrame::OnPageChanged(wxNotebookEvent& event)
{
    DoUpdateAppTitle();
    event.Skip();
}

void MainFrame::OnShiftTab(cb_unused wxCommandEvent& event)
{
    // Must make sure it's cbEditor and not EditorBase
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        ed->DoUnIndent();
}

void MainFrame::OnCtrlAltTab(cb_unused wxCommandEvent& event)
{
    wxCommandEvent dummy;
    switch (m_LastCtrlAltTabWindow)
    {
      case 1:  // Focus is on the Mgmt. panel -> Cycle to Editor
        m_LastCtrlAltTabWindow = 2;
        OnFocusEditor(dummy);
        break;
      case 2:  // Focus is on the Editor -> Cycle to Logs & others
        m_LastCtrlAltTabWindow = 3;
        OnFocusLogsAndOthers(dummy);
        break;
      case 3:  // Focus is on Logs & others -> fall through
      default: // Focus (cycle to) the Mgmt. panel
        m_LastCtrlAltTabWindow = 1;
        OnFocusManagement(dummy);
    }
}

void MainFrame::OnRequestDockWindow(CodeBlocksDockEvent& event)
{
    if (Manager::IsAppShuttingDown())
        return;

    wxAuiPaneInfo info;
    wxString name(event.name);
    if (name.empty())
    {
        static int idx = 0;
        name = wxString::Format("UntitledPane%d", ++idx);
    }

// TODO (mandrav##): Check for existing pane with the same name
    info.Name(name);
    info.Caption(event.title.empty() ? name : event.title);
    switch (event.dockSide)
    {
        case CodeBlocksDockEvent::dsLeft:     info.Left();   break;
        case CodeBlocksDockEvent::dsRight:    info.Right();  break;
        case CodeBlocksDockEvent::dsTop:      info.Top();    break;
        case CodeBlocksDockEvent::dsBottom:   info.Bottom(); break;
        case CodeBlocksDockEvent::dsFloating: info.Float();  break;
        case CodeBlocksDockEvent::dsUndefined: // fall through
        default:                                             break;
    }
    info.Show(event.shown);
    info.BestSize(event.desiredSize);
    info.FloatingSize(event.floatingSize);
    info.FloatingPosition(event.floatingPos);
    info.MinSize(event.minimumSize);
    info.Layer(event.stretch ? 1 : 0);

    if (event.row != -1)
        info.Row(event.row);
    if (event.column != -1)
        info.Position(event.column);
    info.CloseButton(event.hideable ? true : false);
    m_LayoutManager.AddPane(event.pWindow, info);
    DoUpdateLayout();
}

void MainFrame::OnRequestUndockWindow(CodeBlocksDockEvent& event)
{
    wxAuiPaneInfo info = m_LayoutManager.GetPane(event.pWindow);
    if (info.IsOk())
    {
        m_LayoutManager.DetachPane(event.pWindow);
        DoUpdateLayout();
    }
}

void MainFrame::OnRequestShowDockWindow(CodeBlocksDockEvent& event)
{
    m_LayoutManager.GetPane(event.pWindow).Show();
    DoUpdateLayout();

    CodeBlocksDockEvent evt(cbEVT_DOCK_WINDOW_VISIBILITY);
    evt.pWindow = event.pWindow;
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::OnRequestHideDockWindow(CodeBlocksDockEvent& event)
{
    m_LayoutManager.GetPane(event.pWindow).Hide();
    DoUpdateLayout();

    CodeBlocksDockEvent evt(cbEVT_DOCK_WINDOW_VISIBILITY);
    evt.pWindow = event.pWindow;
    Manager::Get()->ProcessEvent(evt);
}

void MainFrame::OnDockWindowVisibility(cb_unused CodeBlocksDockEvent& event)
{
//    if (m_ScriptConsoleID != -1 && event.GetId() == m_ScriptConsoleID)
//        ShowHideScriptConsole();
}

void MainFrame::OnLayoutUpdate(cb_unused CodeBlocksLayoutEvent& event)
{
    DoUpdateLayout();
}

void MainFrame::OnLayoutQuery(CodeBlocksLayoutEvent& event)
{
    event.layout = !m_LastLayoutName.empty() ? m_LastLayoutName : gDefaultLayout;
    event.StopPropagation();
}

void MainFrame::OnLayoutSwitch(CodeBlocksLayoutEvent& event)
{
    LoadViewLayout(event.layout, true);
}

void MainFrame::OnAddLogWindow(CodeBlocksLogEvent& event)
{
    if (Manager::IsAppShuttingDown())
        return;
    wxWindow* p = event.window;
    if (p)
        m_pInfoPane->AddNonLogger(p, event.title, event.icon);
    else
    {
        p = event.logger->CreateControl(m_pInfoPane);
        if (p)
            m_pInfoPane->AddLogger(event.logger, p, event.title, event.icon);
    }
    Manager::Get()->GetLogManager()->NotifyUpdate();
}

void MainFrame::OnRemoveLogWindow(CodeBlocksLogEvent& event)
{
    if (Manager::IsAppShuttingDown())
        return;
    if (event.window)
        m_pInfoPane->DeleteNonLogger(event.window);
    else
        m_pInfoPane->DeleteLogger(event.logger);
}

void MainFrame::OnHideLogWindow(CodeBlocksLogEvent& event)
{
    if (event.window)
        m_pInfoPane->HideNonLogger(event.window);
    else if (event.logger)
        m_pInfoPane->Hide(event.logger);
}

void MainFrame::OnSwitchToLogWindow(CodeBlocksLogEvent& event)
{
    if (event.window)
        m_pInfoPane->ShowNonLogger(event.window);
    else if (event.logger)
        m_pInfoPane->Show(event.logger);
}

void MainFrame::OnGetActiveLogWindow(CodeBlocksLogEvent& event)
{
    bool is_logger;
    int page_index = m_pInfoPane->GetCurrentPage(is_logger);

    event.logger = nullptr;
    event.window = nullptr;

    if (is_logger)
        event.logger = m_pInfoPane->GetLogger(page_index);
    else
        event.window = m_pInfoPane->GetWindow(page_index);
}

void MainFrame::OnShowLogManager(cb_unused CodeBlocksLogEvent& event)
{
    if (!Manager::Get()->GetConfigManager("message_manager")->ReadBool("/auto_hide", false))
        return;

    m_LayoutManager.GetPane(m_pInfoPane).Show(true);
    DoUpdateLayout();
}

void MainFrame::OnHideLogManager(cb_unused CodeBlocksLogEvent& event)
{
    if (!Manager::Get()->GetConfigManager("message_manager")->ReadBool("/auto_hide", false) ||
           m_AutoHideLockCounter > 0)
        return;

    m_LayoutManager.GetPane(m_pInfoPane).Show(false);
    DoUpdateLayout();
}

void MainFrame::OnLockLogManager(cb_unused CodeBlocksLogEvent& event)
{
    if (!Manager::Get()->GetConfigManager("message_manager")->ReadBool("/auto_hide", false))
        return;
    ++m_AutoHideLockCounter;
}

void MainFrame::OnUnlockLogManager(cb_unused CodeBlocksLogEvent& event)
{
    if (!Manager::Get()->GetConfigManager("message_manager")->ReadBool("/auto_hide", false) &&
           m_AutoHideLockCounter > 0)
        return;
    if (--m_AutoHideLockCounter == 0)
    {
        m_LayoutManager.GetPane(m_pInfoPane).Show(false);
        DoUpdateLayout();
    }
}

// Highlight Button
void MainFrame::OnHighlightMenu(cb_unused wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed) return;

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set) return;

    wxMenu* hl = nullptr;
    GetMenuBar()->FindItem(idEditHighlightModeText, &hl);
    if (!hl)
        return;

    wxMenu mm;
    const wxMenuItemList &menuItems = hl->GetMenuItems();

    const wxString selectLanguageName = colour_set->GetLanguageName(ed->GetLanguage());

    for (size_t ii = 0; ii < menuItems.GetCount(); ++ii)
    {
        if (ii > 0 && (ii % 20) == 0)
            mm.Break(); // break into columns every 20 items

        const wxMenuItem *item = menuItems[ii];
        wxMenuItem *newItem = mm.Append(item->GetId(), item->GetItemLabel(), item->GetHelp(),
                                        item->GetKind());
        if (item->GetItemLabel() == selectLanguageName)
            newItem->Check(true);
    }

    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);
    PopupMenu(&mm, GetStatusBar()->GetPosition() + rect.GetPosition());
}

void MainFrame::StartupDone()
{
    m_StartupDone = true;
    DoUpdateLayout();
}

wxStatusBar* MainFrame::OnCreateStatusBar(int number, long style, wxWindowID id, const wxString& name)
{
    MainStatusBar* sb = new MainStatusBar(this, id, style, name);
    cbAssert(number == MainStatusBar::numFields);
    sb->CreateAndFill();
    return sb;
}

// Let the user toggle the toolbar from the context menu
void MainFrame::OnMouseRightUp(wxMouseEvent& event)
{
    PopupToggleToolbarMenu();
    event.Skip();
}

void MainFrame::OnToolBarRightClick(wxCommandEvent& event)
{
    PopupToggleToolbarMenu();
    event.Skip();
}

void MainFrame::PopupToggleToolbarMenu()
{
    wxMenuBar* menuBar = Manager::Get()->GetAppFrame()->GetMenuBar();
    int idx = menuBar->FindMenu(_("&View"));
    if (idx == wxNOT_FOUND)
        return;
    wxMenu* viewMenu = menuBar->GetMenu(idx);
    idx = viewMenu->FindItem(_("Toolbars"));
    if (idx == wxNOT_FOUND)
        return;

    // Clone the View -> Toolbars menu and show it as popup.
    wxMenu* toolbarMenu = viewMenu->FindItem(idx)->GetSubMenu();
    wxMenu menu;
    for (size_t ii = 0; ii < toolbarMenu->GetMenuItemCount(); ++ii)
    {
        wxMenuItem *old = toolbarMenu->FindItemByPosition(ii);
        if (!old)
            continue;
        wxMenuItem *item;
        item = new wxMenuItem(&menu, old->GetId(), old->GetItemLabelText(), old->GetHelp(), old->GetKind());
        menu.Append(item);
    }
    PopupMenu(&menu);
}

void MainFrame::OnGetGlobalAccels(wxCommandEvent& event)
{
    event.SetInt(m_AccelCount);
    void* pUserVector = event.GetClientData();

    // vector<MyClass*>& v = *reinterpret_cast<vector<MyClass*> *>(voidPointerName);
    std::vector<wxAcceleratorEntry>& globalAccels = *reinterpret_cast<std::vector<wxAcceleratorEntry> *>(pUserVector);
    event.SetInt(m_AccelCount);
    for (size_t ii=0; ii < m_AccelCount; ++ii)
        globalAccels.push_back(m_pAccelEntries[ii]);
    return;
}

bool MainFrame::IsLogPaneVisible()
{
    return m_LayoutManager.GetPane(m_pInfoPane).IsShown();
}
