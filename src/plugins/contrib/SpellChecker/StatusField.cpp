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
     m_text(NULL),
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
static wxBitmapBundle LoadImageInPath(const wxString &path, wxString fileName, cb_unused const wxWindow &windowForScaling)
{
    wxString msg;
    wxString dirName;
    wxVector <wxBitmap> bitmaps;

    // Some dictionaries are distributed with hyphens
    wxString fixedFileName(fileName);
    fixedFileName.Replace("-", "_");

    wxDir dir(path);
    if (!dir.IsOpened())
    {
        msg.Printf("Loading images: path '%s' not found!", path);
        Manager::Get()->GetLogManager()->DebugLog(msg);
        return wxBitmapBundle();
    }

    // Scan path looking for "filename" in different sizes
    for (bool cont = dir.GetFirst(&dirName, wxString(), wxDIR_DIRS); cont; cont = dir.GetNext(&dirName))
    {
        const wxString absoluteFileName(path+wxFILE_SEP_PATH+dirName+wxFILE_SEP_PATH+fixedFileName);
        const wxBitmap bmp(cbLoadBitmap(absoluteFileName, wxBITMAP_TYPE_PNG));
        if (bmp.IsOk())
            bitmaps.push_back(bmp);
    }

    if (bitmaps.empty())
        msg.Printf("Loading images: no images found in path '%s'!", path);
    else
        msg.Printf("Loading images: %lu were successfully loaded!", (unsigned long)bitmaps.size());

    Manager::Get()->GetLogManager()->DebugLog(msg);
    return wxBitmapBundle::FromBitmaps(bitmaps);
}
#else
static wxBitmap LoadImageInPath(const wxString &path, wxString fileName, const wxWindow &windowForScaling)
{
    const double actualScaleFactor = cbGetActualContentScaleFactor(windowForScaling);
    const int size = cbFindMinSize16to64(16 * actualScaleFactor);
    const wxString sizePath = wxString::Format("%dx%d", size, size);

    const wxString imgPath(path + wxFILE_SEP_PATH + sizePath + wxFILE_SEP_PATH);

    wxBitmap bmp = cbLoadBitmapScaled(imgPath + fileName, wxBITMAP_TYPE_PNG,
                                      cbGetContentScaleFactor(windowForScaling));
    if (bmp.IsOk())
    {
        const wxString msg(wxString::Format("Loading image: '%s' succeeded!", imgPath + fileName));
        Manager::Get()->GetLogManager()->DebugLog(msg);
        return bmp;
    }

    // some dictionaries are distributed with hyphens
    wxString fileName2(fileName);
    fileName2.Replace("-", "_");

    const wxString msg1(wxString::Format("Loading image: '%s' failed!", imgPath + fileName));
    if (fileName == fileName2)
    {
        Manager::Get()->GetLogManager()->DebugLog(msg1);
        return wxNullBitmap;
    }

    bmp = cbLoadBitmapScaled(imgPath + fileName2, wxBITMAP_TYPE_PNG,
                             cbGetContentScaleFactor(windowForScaling));
    if (!bmp.IsOk())
    {
        const wxString msg2(wxString::Format("Loading image: '%s' failed!", imgPath + fileName2));
        Manager::Get()->GetLogManager()->DebugLog(msg1);
        Manager::Get()->GetLogManager()->DebugLog(msg2);
    }

    const wxString msg(wxString::Format("Loading image: '%s' succeeded!", imgPath + fileName2));
    Manager::Get()->GetLogManager()->DebugLog(msg);

    return bmp;
}
#endif

void SpellCheckerStatusField::Update()
{
    wxString name;
    wxString fileName;

    if (m_sccfg->GetEnableOnlineChecker())
    {
        name = m_sccfg->GetDictionaryName();
        fileName = name + ".png";
    }
    else
    {
        name = _("off");
        fileName = "disabled.png";
    }

    m_text->SetLabel(name);

    wxString bmpPath(m_sccfg->GetRawBitmapPath());
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(bmpPath);

#if wxCHECK_VERSION(3, 1, 6)
    wxBitmapBundle bm(LoadImageInPath(bmpPath, fileName, *this));
#else
    wxBitmap bm(LoadImageInPath(bmpPath, fileName, *this));
#endif

    // Not found?. If name.png is not found and name.length() == 2 try name_NAME.png
    if (!bm.IsOk())
    {
        const wxString languageCode(fileName.BeforeLast('.'));
        if (languageCode.length() == 2)
        {
            const wxString newFileName(languageCode.Lower()+"_"+languageCode.Upper()+".png");
            bm = LoadImageInPath(bmpPath, newFileName, *this);
        }
    }

    // Still not found?. Try in another place
    if (!bm.IsOk())
        bm = LoadImageInPath(m_plugin->GetOnlineCheckerConfigPath(), fileName, *this);

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
#if wxCHECK_VERSION(3, 1, 6)
            const int height = wxRound(GetSize().GetHeight()*cbGetActualContentScaleFactor(*this));
            m_bitmap = new wxStaticBitmap(this, wxID_ANY, bm, wxDefaultPosition, wxSize(height, height));
#else
            m_bitmap = new wxStaticBitmap(this, wxID_ANY, bm);
#endif
            m_bitmap->Connect(wxEVT_LEFT_UP,
                              wxMouseEventHandler(SpellCheckerStatusField::OnPressed), nullptr,
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
    wxSize msize = this->GetSize();

    m_text->SetSize(msize);

    if (m_bitmap)
    {
        wxSize bsize = m_bitmap->GetSize();
        m_bitmap->Move(msize.x/2 - bsize.x/2, msize.y/2 - bsize.y/2);
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
