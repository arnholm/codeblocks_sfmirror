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
    #include <wx/dataobj.h>
    #include <wx/intl.h>
    #include <wx/utils.h>
    #include <wx/sizer.h>
    #include <wx/settings.h>

    #include <manager.h>
    #include <logmanager.h>
    #include <projectmanager.h>
    #include <templatemanager.h>
    #include <pluginmanager.h>
    #include <editormanager.h>
    #include <configmanager.h>
#endif

#include "startherepage.h"
#include "main.h"
#include "appglobals.h"
#include "cbcolourmanager.h"
#include "recentitemslist.h"

#include <wx/clipbrd.h>
#include <wx/docview.h>
#include <wx/wxhtml.h>

int idWin = wxNewId();

wxString GetStartHereTitle()
{
    return _("Start here");
}

class MyHtmlWin : public wxHtmlWindow
{
    public:
        MyHtmlWin(StartHerePage* parent, int id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxHW_SCROLLBAR_AUTO)
            : wxHtmlWindow(parent, id, pos, size, style),
            m_pOwner(parent)
        {
        }

        void OnLinkClicked(const wxHtmlLinkInfo& link) override
        {
            if (m_pOwner)
            {
                if (!m_pOwner->LinkClicked(link))
                    wxLaunchDefaultBrowser(link.GetHref());
            }
        }
    private:
        StartHerePage* m_pOwner;
};

BEGIN_EVENT_TABLE(StartHerePage, EditorBase)
END_EVENT_TABLE()

namespace
{

wxString wrapText(const wxString &text, const wxString &textColour)
{
    return "<font color=\"" + textColour + "\">" + text + "</font>";
}

void ReplaceRecentProjectFiles(wxString &buf, const wxFileHistory &projects, const wxFileHistory &files,
                               const wxString &linkColour, const wxString &textColour)
{
    // replace history vars
    wxString links;

    links << "<table>\n<tr><td colspan=\"2\"><b>";
    links << wrapText(_("Recent projects"), textColour);
    links << "</b></td></tr>\n";
    if (projects.GetCount())
    {
        for (size_t i = 0; i < projects.GetCount(); ++i)
        {
            links << "<tr><td width=\"50\"><img alt=\"\" width=\"20\" src=\"blank.png\" />";
            links << wxString::Format("<a href=\"CB_CMD_DELETE_HISTORY_PROJECT_%zu\"><img alt=\"\" src=\"trash_16x16.png\" /></a>", i + 1);
            links << "<img alt=\"\"  width=\"10\" src=\"blank.png\" /></td><td width=\"10\">";
            links << "<a href=\"CB_CMD_OPEN_HISTORY_PROJECT_" << (i + 1) << "\">"
                  << "<font color=\"" << linkColour << "\">" << projects.GetHistoryFile(i)
                  << "</font></a>";
            links << "</td></tr>\n";
        }
    }
    else
    {
        links << "<tr><td style=\"width:2em;\"></td><td>&nbsp;&nbsp;&nbsp;&nbsp;";
        links << wrapText(_("No recent projects"), textColour);
        links << "</td></tr>\n";
    }

    links << "</table>\n<table>\n<tr><td colspan=\"2\"><b>";
    links << wrapText(_("Recent files"), textColour);
    links << "</b></td></tr>\n";
    if (files.GetCount())
    {
        for (size_t i = 0; i < files.GetCount(); ++i)
        {
            links << "<tr><td width=\"50\"><img alt=\"\" width=\"20\" src=\"blank.png\" />";
            links << wxString::Format("<a href=\"CB_CMD_DELETE_HISTORY_FILE_%zu\"><img alt=\"\" src=\"trash_16x16.png\" /></a>", i + 1);
            links << "<img alt=\"\"  width=\"10\" src=\"blank.png\" /></td><td width=\"10\">";
            links << "<a href=\"CB_CMD_OPEN_HISTORY_FILE_" << (i + 1) << "\">"
                  << "<font color=\"" << linkColour << "\">" << files.GetHistoryFile(i) << "</font></a>";
            links << "</td></tr>\n";
        }
    }
    else
    {
        links << "<tr><td style=\"width:2em;\"></td><td>&nbsp;&nbsp;&nbsp;&nbsp;";
        links << wrapText(_("No recent files"), textColour);
        links << "</td></tr>\n";
    }

    links << "</table>\n";


    // update page
    buf.Replace("CB_VAR_RECENT_FILES_AND_PROJECTS", links);
    buf.Replace("CB_TXT_NEW_PROJECT",    _("Create a new project"));
    buf.Replace("CB_TXT_OPEN_PROJECT",   _("Open an existing project"));
    buf.Replace("CB_TXT_VISIT_FORUMS",   _("Visit the Code::Blocks forums"));
    buf.Replace("CB_TXT_BUG_FEATURE",    _("Report a bug or request a new feature"));
    buf.Replace("CB_TXT_TIP_OF_THE_DAY", _("Tip of the Day"));
}

void CopyToClipboard(const wxString& text)
{
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(text));
        wxTheClipboard->Close();
    }
}

} // anonymous namespace

StartHerePage::StartHerePage(wxEvtHandler* owner, const RecentItemsList &projects,
                             const RecentItemsList &files, wxWindow* parent)
    : EditorBase(parent, GetStartHereTitle(), true),
    m_pOwner(owner),
    m_projects(projects),
    m_files(files)
{
    RegisterColours();

    //ctor
    wxBoxSizer* bs = new wxBoxSizer(wxVERTICAL);

    // avoid gtk-critical because of sizes less than -1 (can happen with wxAuiNotebook/cbAuiNotebook)
    wxSize size = GetSize();
    size.x = std::max(size.x, -1);
    size.y = std::max(size.y, -1);

    m_pWin = new MyHtmlWin(this, idWin, wxPoint(0, 0), size);

    // set default font sizes based on system default font size

    /* NOTE (mandrav#1#): wxWidgets documentation on wxHtmlWindow::SetFonts(),
    states that the sizes array accepts values from -2 to +4.
    My tests (under linux at least) have showed that it actually
    expects real point sizes. */

    wxFont systemFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    int sizes[7];
    for (int i = 0; i < 7; ++i)
        sizes[i] = systemFont.GetPointSize();

    m_pWin->SetFonts(wxEmptyString, wxEmptyString, sizes);

    const wxString filePath(ConfigManager::ReadDataPath());

    // Load an empty page just for setting the default image path.
    // Loading start_here.html instead is slow and generates errors in the debug log
    // because the colour macros have not been translated yet
    m_pWin->LoadPage(filePath+"/start_here.zip#zip:empty_page.html");

    // Read the file so we can perform some search and replace
    wxString buf;
    wxFileSystem* fs = new wxFileSystem;
    wxFSFile* f = fs->OpenFile(filePath+"/start_here.zip#zip:start_here.html");
    if (f)
    {
        char tmp[1024];

        wxInputStream* is = f->GetStream();
        while (!is->Eof() && is->CanRead())
        {
            memset(tmp, 0, sizeof(tmp));
            is->Read(tmp, sizeof(tmp) - 1);
            buf << cbC2U((const char*)tmp);
        }

        delete f;
    }
    else
        buf = _("<html><body><h1>Welcome to Code::Blocks!</h1><br>The default start page seems to be missing...</body></html>");

    delete fs;

#if defined(_LP64) || defined(_WIN64)
    const int bit_type = 64;
#else
    const int bit_type = 32;
#endif

    revInfo.Printf("%s (%s)   ", appglobals::AppActualVersionVerb, ConfigManager::GetSvnDate());

#if defined(__clang__)
    revInfo += wxString::Format("clang %d.%d.%d ", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    revInfo += wxString::Format("gcc %d.%d.%d ", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
    revInfo += wxString::Format("%s/%s - %d bit", appglobals::AppPlatform,
                                appglobals::AppWXAnsiUnicode, bit_type);

    // perform var substitution
    buf.Replace("CB_VAR_REVISION_INFO", revInfo);
    buf.Replace("CB_VAR_VERSION_VERB", appglobals::AppActualVersionVerb);
    buf.Replace("CB_VAR_VERSION", appglobals::AppActualVersion);
    buf.Replace("CB_SAFE_MODE", PluginManager::GetSafeMode() ? _("SAFE MODE") : wxString());

    m_OriginalPageContent = buf; // keep a copy of original for Reload()
    Reload();

    bs->Add(m_pWin, 1, wxEXPAND);
    SetSizer(bs);
    SetAutoLayout(true);
}

StartHerePage::~StartHerePage()
{
    //dtor
    //m_pWin->Destroy();
}

void StartHerePage::RegisterColours()
{
    static bool inited = false;
    if (inited)
        return;
    inited = true;

    ColourManager* cm = Manager::Get()->GetColourManager();
    cm->RegisterColour(_("Start here page"), _("Background colour"), "start_here_background",
                       wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    cm->RegisterColour(_("Start here page"), _("Link colour"), "start_here_link", *wxBLUE);
    cm->RegisterColour(_("Start here page"), _("Text colour"), "start_here_text",
                       wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
}

void StartHerePage::Reload()
{
    // Called every time something in the history changes.
    wxString buf = m_OriginalPageContent;

    ColourManager* cm = Manager::Get()->GetColourManager();
    const wxString &backgroundColour = cm->GetColour("start_here_background").GetAsString(wxC2S_HTML_SYNTAX);
    const wxString &linkColour       = cm->GetColour("start_here_link").GetAsString(wxC2S_HTML_SYNTAX);
    const wxString &textColour       = cm->GetColour("start_here_text").GetAsString(wxC2S_HTML_SYNTAX);

    ReplaceRecentProjectFiles(buf, *m_projects.GetFileHistory(), *m_files.GetFileHistory(), linkColour, textColour);

    buf.Replace("CB_BODY_BGCOLOUR", backgroundColour);
    buf.Replace("CB_LINK_COLOUR",   linkColour);
    buf.Replace("CB_TEXT_COLOUR",   textColour);

    int x, y;
    m_pWin->GetViewStart(&x, &y);
    m_pWin->SetPage(buf);
    m_pWin->Scroll(x, y);
}

bool StartHerePage::LinkClicked(const wxHtmlLinkInfo& link)
{
    //If it's already loading something, stop here
    if (Manager::Get()->GetProjectManager()->IsLoading())
        return true;

    if (!m_pOwner)
        return true;

    wxString href = link.GetHref();
    if (href.StartsWith("CB_CMD_"))
    {
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idStartHerePageLink);
        evt.SetString(link.GetHref());
        wxPostEvent(m_pOwner, evt);
        return true;
    }

    if (   href.IsSameAs("https://www.codeblocks.org/")
        || href.StartsWith("https://sourceforge.net/p/codeblocks/tickets"))
    {
        CopyToClipboard(revInfo);
        return false;
    }

    if (href.IsSameAs("rev"))
    {
        CopyToClipboard(revInfo);
        return true;
    }

    return false;
}
