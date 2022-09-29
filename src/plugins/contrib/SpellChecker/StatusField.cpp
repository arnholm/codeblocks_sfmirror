#include "StatusField.h"

#include <wx/stattext.h>
#include <wx/statbmp.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/filefn.h>

#if wxCHECK_VERSION(3, 1, 6)
#include <wx/dir.h>
#endif

#include <algorithm>
#include <vector>

#include "SpellCheckerConfig.h"
#include "SpellCheckerPlugin.h"

#include <logmanager.h>
#include <macrosmanager.h>

#define LANGS 10

namespace
{
const int idCommand[LANGS]  = {static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),
                               static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId()),static_cast<int>(wxNewId())
                              };
const int idEnableSpellCheck = wxNewId();
const int idEditPersonalDictionary = wxNewId();
};

SpellCheckerStatusField::SpellCheckerStatusField(wxWindow* parent, SpellCheckerPlugin *plugin, SpellCheckerConfig *sccfg)
    :wxPanel(parent, wxID_ANY),
     m_bitmap(NULL),
     m_sccfg(sccfg),
     m_plugin(plugin)
{
    //ctor
    m_text = new wxStaticText(this, wxID_ANY, m_sccfg->GetDictionaryName());

    Update();

    Connect(wxEVT_SIZE, wxSizeEventHandler(SpellCheckerStatusField::OnSize), NULL, this);
    Connect(idCommand[0],idCommand[LANGS-1], wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnSelect), NULL, this);
    Connect(idEnableSpellCheck, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnSelect), NULL, this);
    Connect(idEditPersonalDictionary, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnEditPersonalDictionary), NULL, this);

    m_text->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(SpellCheckerStatusField::OnPressed), NULL,
                    this);
    Connect(wxEVT_LEFT_UP, wxMouseEventHandler(SpellCheckerStatusField::OnPressed), NULL, this);
}

SpellCheckerStatusField::~SpellCheckerStatusField()
{
    //dtor
    Disconnect(wxEVT_SIZE, wxSizeEventHandler(SpellCheckerStatusField::OnSize), NULL, this);
    Disconnect(idCommand[0],idCommand[LANGS-1], wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnSelect), NULL, this);
    Disconnect(idEnableSpellCheck, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnSelect), NULL, this);
    Disconnect(idEditPersonalDictionary, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SpellCheckerStatusField::OnEditPersonalDictionary), NULL, this);

    m_text->Disconnect(wxEVT_LEFT_UP, wxMouseEventHandler(SpellCheckerStatusField::OnPressed));
    if (m_bitmap)
    {
        m_bitmap->Disconnect(wxEVT_LEFT_UP,
                             wxMouseEventHandler(SpellCheckerStatusField::OnPressed));
    }
    Disconnect(wxEVT_LEFT_UP, wxMouseEventHandler(SpellCheckerStatusField::OnPressed));
}

#if wxCHECK_VERSION(3, 1, 6)
static wxBitmapBundle LoadImageInPath(const wxString& path, const wxString& fileName, const wxSize& size)
{
    const wxString imgPath(path+"/svg/");
    wxBitmapBundle bmp = cbLoadBitmapBundleFromSVG(imgPath+fileName, size);
    if (!bmp.IsOk())
        Manager::Get()->GetLogManager()->Log(wxString::Format(_("Loading image: '%s' failed!"), imgPath+fileName));

    return bmp;
}
#else
static wxBitmap LoadImageInPath(const wxString& path, const wxString& fileName, const wxSize& size)
{
    const wxString imgPath(path+wxString::Format("/%dx%d/", size.GetWidth(), size.GetHeight()));
    wxBitmap bmp = cbLoadBitmap(imgPath+fileName, wxBITMAP_TYPE_PNG);
    if (!bmp.IsOk())
        Manager::Get()->GetLogManager()->Log(wxString::Format(_("Loading image: '%s' failed!"), imgPath+fileName));

    return bmp;
}
#endif

void SpellCheckerStatusField::Update()
{
    wxString name;
    wxString fileName;

#if wxCHECK_VERSION(3, 1, 6)
    wxBitmapBundle bm;
    const wxString ext(".svg");
#else
    wxBitmap bm;
    const wxString ext(".png");
#endif

    if (m_sccfg->GetEnableOnlineChecker())
    {
        name = m_sccfg->GetDictionaryName();
        fileName = name + ext;
        // Some dictionaries are distributed with hyphens
        fileName.Replace("-", "_");
    }
    else
    {
        name = _("off");
        fileName = "disabled"+ext;
    }

    m_text->SetLabel(name);

    wxString bmpPath(m_sccfg->GetRawBitmapPath());
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(bmpPath);

    // Get bitmap size
#if wxCHECK_VERSION(3, 1, 6)
    const int height = 20;
#else
    const int height = cbFindMinSize16to64(wxRound(20*cbGetContentScaleFactor(*this)));
#endif
    const wxSize bmpSize(height, height);

    // Try loading
    bm = LoadImageInPath(bmpPath, fileName, bmpSize);

    // Not found?. If name.xxx is not found and name.length() == 2 try name_NAME.xxx
    if (!bm.IsOk())
    {
        const wxString languageCode(fileName.BeforeLast('.'));
        if (languageCode.length() == 2)
        {
            const wxString newFileName(languageCode.Lower()+"_"+languageCode.Upper()+ext);
            bm = LoadImageInPath(bmpPath, newFileName, bmpSize);
        }
    }

    // Still not found?. Try in another place
    if (!bm.IsOk())
        bm = LoadImageInPath(m_plugin->GetOnlineCheckerConfigPath(), fileName, bmpSize);

    if (bm.IsOk())
    {
        m_text->Hide();
        if (m_bitmap)
        {
            m_bitmap->Hide();
            m_bitmap->SetBitmap(bm);
            m_bitmap->Show();
        }
        else
        {
            m_bitmap = new wxStaticBitmap(this, wxID_ANY, bm);
            m_bitmap->Connect(wxEVT_LEFT_UP,
                              wxMouseEventHandler(SpellCheckerStatusField::OnPressed),
                              nullptr,
                              this);
        }
    }
    else
    {
        if (m_bitmap)
            m_bitmap->Hide();

        m_text->Show();
    }

    DoSize();
}

void SpellCheckerStatusField::OnSize(cb_unused wxSizeEvent &event)
{
    DoSize();
}

void SpellCheckerStatusField::DoSize()
{
    const wxSize msize(GetSize());
    m_text->SetSize(msize);
    if (m_bitmap)
    {
        const wxSize bsize(m_bitmap->GetSize());
        m_bitmap->Move((msize.GetWidth()-bsize.GetWidth())/2, (msize.GetHeight()-bsize.GetHeight())/2);
    }
}

void SpellCheckerStatusField::OnPressed(cb_unused wxMouseEvent &event)
{
    m_sccfg->ScanForDictionaries();
    wxMenu *popup = new wxMenu();
    std::vector<wxString> dicts = m_sccfg->GetPossibleDictionaries();
    for ( unsigned int i = 0 ; i < dicts.size() && i < LANGS ; i++ )
        popup->Append( idCommand[i], m_sccfg->GetLanguageName(dicts[i]), _T(""), wxITEM_CHECK)->Check(dicts[i] == m_sccfg->GetDictionaryName() );
    if (!dicts.empty())
        popup->AppendSeparator();
    popup->Append(idEnableSpellCheck, _("Enable spell check"), wxEmptyString, wxITEM_CHECK)->Check(m_sccfg->GetEnableOnlineChecker());
    wxMenuItem *mnuItm = popup->Append( idEditPersonalDictionary, _("Edit personal dictionary"), _T(""));
    mnuItm->Enable( wxFile::Exists(m_sccfg->GetPersonalDictionaryFilename()) );

    PopupMenu(popup);
    delete popup;
}

void SpellCheckerStatusField::OnSelect(wxCommandEvent &event)
{
    unsigned int idx;
    for ( idx = 0 ; idx < LANGS ; idx++)
        if ( event.GetId() == idCommand[idx])
            break;

    std::vector<wxString> dicts = m_sccfg->GetPossibleDictionaries();

    if ( idx < dicts.size() )
    {
        m_sccfg->SetDictionaryName(dicts[idx]);
        m_sccfg->SetEnableOnlineChecker(true);
        m_sccfg->Save(); // save it
    }
    else if (!dicts.empty() && event.GetId() == idEnableSpellCheck)
    {
        m_sccfg->SetEnableOnlineChecker(!m_sccfg->GetEnableOnlineChecker()); // toggle
        if (   m_sccfg->GetEnableOnlineChecker()
                && std::find(dicts.begin(), dicts.end(), m_sccfg->GetDictionaryName()) == dicts.end() )
        {
            // insure there always is a valid dictionary selected when enabled
            m_sccfg->SetDictionaryName(dicts[0]);
        }
        m_sccfg->Save();
    }
}

void SpellCheckerStatusField::OnEditPersonalDictionary(cb_unused wxCommandEvent &event)
{
    m_plugin->EditPersonalDictionary();
}
