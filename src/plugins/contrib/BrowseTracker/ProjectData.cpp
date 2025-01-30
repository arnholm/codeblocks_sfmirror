/*
	This file is part of Browse Tracker, a plugin for Code::Blocks
	Copyright (C) 2007 Pecan Heber

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// RCS-ID: $Id$

#if defined(CB_PRECOMP)
#include "sdk.h"
#else
	#include "sdk_events.h"
	#include "manager.h"
	#include "editormanager.h"
	#include "editorbase.h"
	#include "cbeditor.h"
	#include "cbproject.h"
#endif

    #include <wx/dynarray.h> //for wxArray and wxSortedArray
    #include <projectloader_hooks.h>
    #include <editor_hooks.h>
    #include "personalitymanager.h"
	#include <wx/stdpaths.h>
	#include <wx/app.h>
	#include <wx/menu.h>

#include "Version.h"
#include "BrowseMarks.h"
#include "BrowseTrackerDefs.h"
#include "ProjectData.h"
#include "BrowseTrackerLayout.h"
#include "helpers.h"

// ----------------------------------------------------------------------------
ProjectData::ProjectData()
// ----------------------------------------------------------------------------
{
    //private ctor
}
// ----------------------------------------------------------------------------
ProjectData::ProjectData(cbProject* pcbProject)
// ----------------------------------------------------------------------------
{
    //ctor
    #if defined(LOGGING)
    if (not pcbProject) asm("int3"); /*trap*/;
    #endif
    if (not pcbProject) return;
    m_pCBProject = pcbProject;
    m_ProjectFilename = pcbProject->GetFilename();
    m_CurrIndexEntry = 0;
    m_LastIndexEntry = Helpers::GetMaxAllocEntries()-1;
    m_pEdMgr = Manager::Get()->GetEditorManager();
    m_ActivationCount = 0;
    m_bLayoutLoaded = false;
    LoadLayout();
}
// ----------------------------------------------------------------------------
ProjectData::~ProjectData()
// ----------------------------------------------------------------------------
{
    //dtor

    // *BrowseMarksArchive* release the editor layout hash table ptrs to BrowseMarks
    for (FileBrowse_MarksHash::iterator it = m_FileBrowse_MarksArchive.begin(); it != m_FileBrowse_MarksArchive.end(); ++it)
    {
        delete it->second;
    }
    m_FileBrowse_MarksArchive.clear();


}
// ----------------------------------------------------------------------------
void ProjectData::IncrementActivationCount()
// ----------------------------------------------------------------------------
{
    m_ActivationCount += 1;
}
// ----------------------------------------------------------------------------
int ProjectData::GetActivationCount()
// ----------------------------------------------------------------------------
{
     return m_ActivationCount++;
}
// ----------------------------------------------------------------------------
wxString ProjectData::GetProjectFilename()
// ----------------------------------------------------------------------------
{
    return m_ProjectFilename;
}
// ----------------------------------------------------------------------------
void ProjectData::AddEditor( wxString /*filePath */)
// ----------------------------------------------------------------------------
{
////    // Don't stow duplicates
////    if ( m_EditorBaseArray.Index( eb ) != wxNOT_FOUND ) return;
////
////    // not found, stow new data into arrays
////    int index = m_LastIndexEntry;
////    if (++index >= Helpers::GetMaxEntries()) index = 0;
////    m_LastIndexEntry = index;
////    m_EditorBaseArray[index] = eb;
////    m_cbEditorArray[index] = cbed;
////    m_cbSTCArray[index] = control;

}
// ----------------------------------------------------------------------------
bool ProjectData::FindFilename( const wxString filePath)
// ----------------------------------------------------------------------------
{
    //Return true if we own a BrowseMarks array by this filepath;

    FileBrowse_MarksHash& hash = m_FileBrowse_MarksArchive;
    FileBrowse_MarksHash::iterator it = hash.find(filePath);
    if ( it == hash.end() ) {
        //DumpHash(wxT("BrowseMarks"));
        return false;
    }
    return true;
}
// ----------------------------------------------------------------------------
BrowseMarks* ProjectData::GetBrowse_MarksFromHash( wxString filePath)
// ----------------------------------------------------------------------------
{
    // Return a pointer to a BrowseMarks array with this filePath

    FileBrowse_MarksHash& hash = m_FileBrowse_MarksArchive;
    return GetPointerToBrowse_MarksArray(hash, filePath);
}
// ----------------------------------------------------------------------------
BrowseMarks* ProjectData::GetPointerToBrowse_MarksArray(FileBrowse_MarksHash& hash, wxString filePath)
// ----------------------------------------------------------------------------
{
    // Return a pointer to a BrowseMarks array with this filePath

    for (FileBrowse_MarksHash::iterator it = hash.begin(); it != hash.end(); it++)
    {
        BrowseMarks* p = it->second;
        if ( p->GetFilePath() == filePath ) {return p;}
    }

    return 0;
}
// ----------------------------------------------------------------------------
BrowseMarks* ProjectData::HashAddBrowse_Marks( const wxString fullPath )
// ----------------------------------------------------------------------------
{
    // EditorManager calls fail during the OnEditorClose event
    //EditorBase* eb = Manager::Get()->GetEditorManager()->GetEditor(filename);

    EditorBase* eb = m_pEdMgr->GetEditor(fullPath);
    #if defined(LOGGING)
        if(not eb) asm("int3"); /*trap*/
    #endif
    if(not eb) return 0;
    wxString filePath = eb->GetFilename();
    if (filePath.IsEmpty()) return 0;
    // don't add duplicates
    BrowseMarks* pBrowse_Marks = GetBrowse_MarksFromHash( filePath );
    if (pBrowse_Marks) return pBrowse_Marks ;
    pBrowse_Marks = new BrowseMarks( fullPath );
    if (pBrowse_Marks) m_FileBrowse_MarksArchive[filePath] = pBrowse_Marks;

    return pBrowse_Marks;
}
// ----------------------------------------------------------------------------
void ProjectData::LoadLayout()
// ----------------------------------------------------------------------------
{
    // Load a layout file for this project
    #if defined(LOGGING)
    LOGIT( _T("ProjectData::LoadLayout()for[%s]"),m_ProjectFilename.c_str() );
    #endif

    if (m_ProjectFilename.IsEmpty())
        return ;

    wxFileName fname(m_ProjectFilename);
    fname.SetExt(_T("bmarks"));
    BrowseTrackerLayout layout( m_pCBProject );
    layout.Open(fname.GetFullPath(), m_FileBrowse_MarksArchive );
    m_bLayoutLoaded = true;
}
// ----------------------------------------------------------------------------
void ProjectData::SaveLayout()
// ----------------------------------------------------------------------------
{
    // Write a layout file for this project
    #if defined(LOGGING)
    LOGIT( _T("ProjectData::SAVELayout()") );
    #endif

    if (m_ProjectFilename.IsEmpty())
        return ;

    wxFileName fname(m_ProjectFilename);
    fname.SetExt(_T("bmarks"));
    BrowseTrackerLayout layout( m_pCBProject );
    //DumpBrowse_Marks(wxT("BrowseMarks"));
    layout.Save(fname.GetFullPath(), m_FileBrowse_MarksArchive );


    // *Testing* See if cbEditor is actually there
    //EditorBase* eb = m_EditorBaseArray[1];
    //cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
    //cbStyledTextCtrl* control = cbed->GetControl();
    //#if defined(LOGGING)
    //LOGIT( _T("eb[%p]cbed[%p]control[%p]"), eb, cbed, control );
    //#endif

    // *Testing* Check against our array
    //eb = m_EditorBaseArray[1];
    //cbed = m_cbEditorArray[1];
    //control = m_cbSTCArray[1];
    //#if defined(LOGGING)
    //LOGIT( _T("eb[%p]cbed[%p]control[%p]"), eb, cbed, control );
    //#endif


}
// ----------------------------------------------------------------------------
void ProjectData::DumpHash( const wxString
#if defined(LOGGING)
	hashType
#endif
)
// ----------------------------------------------------------------------------
{

    #if defined(LOGGING)

    FileBrowse_MarksHash* phash = &m_FileBrowse_MarksArchive;
    FileBrowse_MarksHash& hash = *phash;

    LOGIT(wxString::Format("--- DumpProjectHash ---[%s]Count[%zu]Name[%s]", hashType, hash.size(), m_ProjectFilename));
    for (FileBrowse_MarksHash::iterator it = hash.begin(); it != hash.end(); it++)
    {
        wxString filename = it->first; //an Editor filename withing this project
        BrowseMarks* p = it->second;    // ptr to array of Editor Browse/Book mark cursor positions
        LOGIT(wxString::Format("filename[%s]BrowseMark*[%p]name[%s]", filename, p, p->GetFilePath()));
    }

    #endif
}

// ----------------------------------------------------------------------------
void ProjectData::DumpBrowse_Marks( const wxString
#if defined(LOGGING)
	hashType
#endif
)
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
    LOGIT( _T("--- DumpBrowseData ---[%s]"), hashType.c_str()  );

    FileBrowse_MarksHash* phash = &m_FileBrowse_MarksArchive;
    FileBrowse_MarksHash& hash = *phash;

    LOGIT(wxString::Format("Dump_%s Size[%zu]", hashType, hash.size()));

    for (FileBrowse_MarksHash::iterator it = hash.begin(); it != hash.end(); ++it)
    {
        wxString filename = it->first;
        BrowseMarks* p = it->second;
        LOGIT(wxString::Format("Filename[%s]%s*[%p]name[%s]", filename, hashType, p, (p ? p->GetFilePath() : wxString())));
        if (p)
        {
            //dump the browse marks
            p->Dump();
        }
    }

    #endif
}
// ----------------------------------------------------------------------------
// 2008/01/23 MortenMacFly
// ----------------------------------------------------------------------------
// I have serious issues with this plugin (recently). Whenever I try to import a
// Visual Studio solution C::B crashes. The reason is the BT plugin, file
// ProjectData.cpp, line 63 (m_ProjectFilename = pcbProject->GetFilename();).
// It seems pcbProject *is* NULL sometimes in the case of an import.
// So you better don't rely on it. I had to disable the BT plugin to be able to
// import VS solutions again. Sad
