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
#endif

#include "BrowseTracker.h"
#include "cbstyledtextctrl.h"
#include "BrowseMarks.h"
#include "BrowseTrackerDefs.h"
#include "helpers.h"
//#include "Version.h"

// ----------------------------------------------------------------------------
BrowseMarks::BrowseMarks()
// ----------------------------------------------------------------------------
{
    //ctor private and unused
}
// ----------------------------------------------------------------------------
BrowseMarks::~BrowseMarks()
// ----------------------------------------------------------------------------
{
    //dtor
    m_EdPosnArray.Clear();
}
// ----------------------------------------------------------------------------
BrowseMarks::BrowseMarks(wxString fullPath )
// ----------------------------------------------------------------------------
{
    //ctor
    #if defined(LOGGING)
        //if ( fullPath.IsEmpty() ) {asm("int3"); }
    #endif
    wxFileName fname(fullPath);
    if ( fullPath.IsEmpty() )
        fname.Assign(wxT("Created.with.MissingFileName"));
    m_filePath = fname.GetFullPath();
    m_fileShortName = fname.GetFullName();

    //#if defined(LOGGING)
    //LOGIT( _T("BT BrowseMarks[%s][%s]"),m_filePath.c_str() ,m_fileShortName.c_str() );
    //#endif

    m_EdPosnArray.SetCount(Helpers::GetMaxAllocEntries(), -1);     //patch 2886
    m_currIndex = 0;    //index of current entry
    m_lastIndex = Helpers::GetMaxAllocEntries()-1;    //index for insertion of new browse marks
    m_pEdMgr = Manager::Get()->GetEditorManager();
}
// ----------------------------------------------------------------------------
void BrowseMarks::SetBrowseMarksStyle( int userStyle )
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
    LOGIT( _T("BT BrowseMarks::SetBrowseMarksStyle[%d]"), userStyle );
    #endif
    EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
    #if defined(LOGGING)
        //if (not eb) asm("int3"); /*trap*/
    #endif
    if (not eb) return;
    cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
    if (not cbed) return;
    cbStyledTextCtrl* pControl = cbed->GetControl();

    int OldBrowseMarkerId = GetBrowseMarkerId();
    //int OldBrowseMarkerStyle = GetBrowseMarkerStyle();

    switch( userStyle )
    {
//        case BrowseMarksStyle:
//        {
//            gBrowse_MarkerId = BROWSETRACKER_MARKER ;
//            gBrowse_MarkerStyle  = BROWSETRACKER_MARKER_STYLE;
//            break;
//        }
        case BookMarksStyle:
        {
            gBrowse_MarkerId = BOOKMARK_MARKER;
            gBrowse_MarkerStyle  = BOOKMARK_STYLE;
            break;
        }
        case HiddenMarksStyle:
        {
            gBrowse_MarkerId = BROWSETRACKER_MARKER;
            gBrowse_MarkerStyle  = BROWSETRACKER_HIDDEN_STYLE;
            break;
        }
        default:
            break;
    }//switch

    // Define scintilla BrowseTracker margin marker
    int marginMask = pControl->GetMarginMask(1);
    pControl->SetMarginMask( 1, marginMask | (1 << GetBrowseMarkerId()) );
    pControl->MarkerDefine( GetBrowseMarkerId(), GetBrowseMarkerStyle() );
    // the following stmt seems to do nothing for wxSCI_MARK_DOTDOTDOT
    pControl->MarkerSetBackground( GetBrowseMarkerId(), wxColour(0xA0, 0xA0, 0xFF));
    #if defined(LOGGING)
    LOGIT( _T("BT BrowseMarks::UserStyle[%d]MarkerId[%d]MarkerStyle[%d]"),userStyle,GetBrowseMarkerId(), GetBrowseMarkerStyle() );
    #endif

    // When the Marker types only change style, our work is done.
    // Scintilla did the work for us.
    #if defined(LOGGING)
        LOGIT( _T("BT OldBrowseMarkerId[%d] NewBrowseMarkerId[%d]"), OldBrowseMarkerId, GetBrowseMarkerId() );
    #endif
    if ( OldBrowseMarkerId == GetBrowseMarkerId() )
        return;
    // Old and new Marker types are different, remove the old and place the
    // new in its place.
    RemoveMarkerTypes( OldBrowseMarkerId);
    PlaceMarkerTypes( GetBrowseMarkerId() );

}//SetBrowseMarksStyle
// ----------------------------------------------------------------------------
int BrowseMarks::GetMarkPrevious()
// ----------------------------------------------------------------------------
{
    // m_EdPosnArray contains editor line numbers
    // Get the editor line number for the previous marked line

    int index = m_currIndex;
    int oldPos = m_EdPosnArray[index];
    if ((--index) < 0) index = Helpers::GetMaxAllocEntries()-1;
    int newPos = m_EdPosnArray[index] ;
    int maxEntries = Helpers::GetMaxAllocEntries();
    // skip over duplicate positions
    for (int i=0; i<maxEntries;++i)
    {
        if ((newPos != -1) && (newPos != oldPos)) break;
        if ( (--index) < 0 ) index = Helpers::GetMaxAllocEntries()-1;
        newPos = m_EdPosnArray[index] ;
    }
    if (newPos == -1) return oldPos;
    m_currIndex = index;
    return newPos;
}
// ----------------------------------------------------------------------------
int BrowseMarks::GetMarkNext()
// ----------------------------------------------------------------------------
{
    // m_EdPOsnArray contains editor line numbers
    // Get the editor line number for the next marked line

    int index = m_currIndex;
    int oldPos = m_EdPosnArray[index];
    if ( (++index) >= Helpers::GetMaxAllocEntries()) index = 0;
    int newPos = m_EdPosnArray[index] ;
    int maxEntries = Helpers::GetMaxAllocEntries();
    // skip over duplicate positions
    for (int i=0; i<maxEntries; ++i)
    {
        if ((newPos != oldPos) && (newPos != -1)) break;
        if ( (++index) >= Helpers::GetMaxAllocEntries() ) index = 0;
        newPos = m_EdPosnArray[index] ;
    }
    if (newPos == -1) return oldPos;
    m_currIndex = index;
    return newPos;
}
// ----------------------------------------------------------------------------
int BrowseMarks::GetMarkCurrent()
// ----------------------------------------------------------------------------
{
    return m_EdPosnArray[m_currIndex];
}
// ----------------------------------------------------------------------------
int BrowseMarks::FindMark(int Posn)
// ----------------------------------------------------------------------------
{
    // Search for editor line number (posn) in editor lines array
    // return the index of the entry holding that line number

    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i < maxEntries; ++i )
    	if (Posn == m_EdPosnArray[i]) return i;
    return -1;
}
// ----------------------------------------------------------------------------
int BrowseMarks::GetMark(int index)
// ----------------------------------------------------------------------------
{
    // return the editor line number held by array[index]

    if ((index < 0) || ( index >= Helpers::GetMaxAllocEntries()) )
        return -1;
    return m_EdPosnArray[index];
}
// ----------------------------------------------------------------------------
int BrowseMarks::GetMarkCount()
// ----------------------------------------------------------------------------
{
    // return count of array items != -1 (empty slot)
    // Count BrowseMarks
    int count = 0;
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i < maxEntries; ++i )
        if (m_EdPosnArray[i] not_eq -1) ++count;

    return count;
}
// ----------------------------------------------------------------------------
void BrowseMarks::CopyMarksFrom(const BrowseMarks& otherBrowse_Marks)
// ----------------------------------------------------------------------------
{
    // Duplicate a BrowseMarks array

    #if defined(LOGGING)
    //LOGIT( _T("BT CopyMarksFrom BrowseMarks[%p]To[%p]"), &otherBrowse_Marks, this );
    #endif
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i<maxEntries; ++i)
    {
        m_EdPosnArray[i] = otherBrowse_Marks.m_EdPosnArray[i];
        //#if defined(LOGGING)
        //LOGIT( _T("BT CopyMarksFrom lhs[%d]rhs[%d]"),m_EdPosnArray[i],otherBrowse_Marks.m_EdPosnArray[i]  );
        //#endif
    }
    //#if defined(LOGGING)
    //LOGIT( _T("BT CopyMarksFrom[%s]To[%s]"),
    //            ((BrowseMarks&)otherBrowse_Marks).GetFilePath().c_str(),
    //            GetFilePath().c_str() );
    //#endif
    //Dump();
}
// ----------------------------------------------------------------------------
void BrowseMarks::ImportBrowse_Marks()
// ----------------------------------------------------------------------------
{
    // rebuild BrowseMarks from scintilla browse marks

    #if defined(LOGGING)
    //LOGIT( _T("BT ImportBrowse_Marks") );
    #endif

    //-EditorBase* eb = m_pEditorBase ;
    EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
    #if defined(LOGGING)
    LOGIT(_T("BT ImportBrowse_Marks entered with no active editor base."));
    #endif
    // This can happens when editing script for example .
    if (not eb) return;
    cbEditor* pcbEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor( eb ) ;
    if (not pcbEditor) return;
    ClearAllBrowse_Marks();
    cbStyledTextCtrl* pControl = pcbEditor->GetControl();
    //-int line = pControl->MarkerNext( 0, 1<<BROWSETRACKER_MARKER);
    int line = pControl->MarkerNext( 0, 1<<GetBrowseMarkerId());
    while( line != -1)
    {
        int posn = pControl->PositionFromLine( line );
        RecordMark(  posn );
        //-line = pControl->MarkerNext( ++line,  1<<BROWSETRACKER_MARKER);
        line = pControl->MarkerNext( ++line,  1<<GetBrowseMarkerId());
    }
}//ImportBrowse_Marks
// ----------------------------------------------------------------------------
void BrowseMarks::RecordMarksFrom(BrowseMarks& otherBrowse_Marks)
// ----------------------------------------------------------------------------
{
    // get editor by filename in case the editor was close/opened again

    EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
    // Let's get paranoid here, since a crash was reported Nov. 2021
    // https://forums.codeblocks.org/index.php?topic=24716.msg168611#msg168611
    cbAssertNonFatal(eb != nullptr);
    #if defined(LOGGING)
        //if (not eb) asm("int3"); /*trap*/
    #endif
    if (not eb) return;

    cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
    cbAssertNonFatal(cbed != nullptr);
    if (not cbed) return;

    cbStyledTextCtrl* control = cbed->GetControl();
    if (not control) return; //2021/11/27 avoid possible crash

    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i<maxEntries; ++i)
    {
        int posn = otherBrowse_Marks.GetMark(i);
        if ( posn != -1 )
        {
            #if defined(LOGGING)
                //LOGIT( _T("RecordMarksFrom for[%d][%d]"),i,posn );
            #endif
            RecordMark( posn );
            int line = control->LineFromPosition(posn);
            control->MarkerAdd(line, GetBrowseMarkerId());
        }
    }
}
// ----------------------------------------------------------------------------
void BrowseMarks::RecordMark(int pos)
// ----------------------------------------------------------------------------
{
    // Record the new position

    int index = m_lastIndex;
    ++index;
    if (index >= Helpers::GetMaxAllocEntries()) index = 0;
    m_EdPosnArray[index] = pos;
    m_lastIndex = index;
    m_currIndex = index;
    #ifdef LOGGING
      cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
      if (cbed)
      {     cbStyledTextCtrl* control = cbed->GetControl();
            int line = control->LineFromPosition(pos);
            wxUnusedVar(line);
            //LOGIT( _T("RecordMark index[%d]Line[%d]pos[%d]"), m_currIndex, line, pos );
      }
      else{
          //LOGIT( _T("RecordMark index[%d]pos[%d]"), m_currIndex, pos );
      }
    #endif
}
// ----------------------------------------------------------------------------
void BrowseMarks::ClearAllBrowse_Marks()
// ----------------------------------------------------------------------------
{
    // Clear all browse marks
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i < maxEntries; ++i )
    	m_EdPosnArray[i] = -1;
    //-m_EdPosnArray[0] = currPosn; <- causes scintilla marks to get out of sync
    m_currIndex = 0;
    m_lastIndex = Helpers::GetMaxAllocEntries()-1;
}
// ----------------------------------------------------------------------------
void BrowseMarks::ClearMark(int startPos, int endPos)
// ----------------------------------------------------------------------------
{
    // clear any line with startPos through endPos inclusive from the array

        int maxEntries = Helpers::GetMaxAllocEntries();
        for (int i=0; i<maxEntries; ++i)
        {
            if (( m_EdPosnArray[i] >= startPos) && (m_EdPosnArray[i] <= endPos) )
                m_EdPosnArray[i] = -1;
        }
}
// ----------------------------------------------------------------------------
bool BrowseMarks::LineHasMarker(cbStyledTextCtrl* pControl, int line, int markerId) const
// ----------------------------------------------------------------------------
{
    // Return true if the editor line has a margin marker icon

    if (line == -1)
        line = pControl->GetCurrentLine();
    //-return pControl->MarkerGet(line) & (BROWSETRACKER_MARKER_MASK);
    return pControl->MarkerGet(line) & (1<<markerId);
}
// ----------------------------------------------------------------------------
void BrowseMarks::MarkRemove(cbStyledTextCtrl* pControl, int line, int markerId)
// ----------------------------------------------------------------------------
{
    // Remove the line margin marker icon
    if (line == -1)
        line = pControl->GetCurrentLine();
    if (LineHasMarker(pControl, line, markerId))
        //-pControl->MarkerDelete(line, BROWSETRACKER_MARKER);
        pControl->MarkerDelete(line, markerId);
}
// ----------------------------------------------------------------------------
void BrowseMarks::MarkLine(cbStyledTextCtrl* pControl, int line, int markerId)
// ----------------------------------------------------------------------------
{
    // Set the editor line margin marker icon

    if (not pControl) return;
    if (line <= -1)   return;
    else
        pControl->MarkerAdd(line, markerId);
}
// ----------------------------------------------------------------------------
void BrowseMarks::RebuildBrowse_Marks(cbEditor* cbed, bool addedLines)
// ----------------------------------------------------------------------------
{
    // Rebuild BrowseMarks shadowing scintilla when lines added/deleted

    #if defined(LOGGING)
    //LOGIT( _T("--RebuildBrowse_Marks--") );
    #endif
    if (not cbed) return;
    cbStyledTextCtrl* control = cbed->GetControl();
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i=0; i<maxEntries; ++i)
    {
        if ( m_EdPosnArray[i] == -1 ) continue;
        int line = control->LineFromPosition( m_EdPosnArray[i]);
        if ( line == -1 ) {m_EdPosnArray[i] = -1; continue;}
        if ( LineHasMarker( control, line, GetBrowseMarkerId()) ) {continue;}
        if ( addedLines )
            //-{line = control->MarkerNext( line, BROWSETRACKER_MARKER_MASK );}
            {line = control->MarkerNext( line, (1<<GetBrowseMarkerId()) );}
        else
            //-{line = control->MarkerPrevious( line,BROWSETRACKER_MARKER_MASK );}
            {line = control->MarkerPrevious( line,(1<<GetBrowseMarkerId()) );}
        if ( line == -1 ) {m_EdPosnArray[i] = -1; continue;}
        m_EdPosnArray[i] = control->PositionFromLine(line);
    }
}
// ----------------------------------------------------------------------------
wxString BrowseMarks::GetStringOfBrowse_Marks() const
// ----------------------------------------------------------------------------
{
    // return a comma delimited string of browse marks

    wxString browseMarks = wxT("");
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i = 0; i< maxEntries; ++i)
    {
        if ( m_EdPosnArray[i] == -1 ) continue;
        if (not browseMarks.IsEmpty())  browseMarks << wxT(",") ;
        browseMarks = browseMarks << wxString::Format(wxT("%i"), m_EdPosnArray[i]);
        #if defined(LOGGING)
        //LOGIT( _T("EdPosnArray[%d]str[%s]"),m_EdPosnArray[i], browseMarks.c_str()  );
        #endif
    }
    return browseMarks;
}
// ----------------------------------------------------------------------------
void BrowseMarks::RemoveMarkerTypes( int markerId )
// ----------------------------------------------------------------------------
{
    // Remove scintilla markers

    EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
    cbEditor* cbed = 0;
    cbStyledTextCtrl* control = 0;
    //cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (eb) cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
    if (cbed) control = cbed->GetControl();
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i = 0; i < maxEntries; ++i )
    {
            const int pos = m_EdPosnArray[i];
            int line = -1;
            if (control && (pos != -1))
                line = control->LineFromPosition(pos);
            if (-1 not_eq line)
                if ( LineHasMarker(control, line, markerId ))
                    MarkRemove(control, line, markerId);
    }
}
// ----------------------------------------------------------------------------
void BrowseMarks::PlaceMarkerTypes( int markerId )
// ----------------------------------------------------------------------------
{
    // Remove scintilla markers

    EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
    cbEditor* cbed = 0;
    cbStyledTextCtrl* control = 0;
    //cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (eb) cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
    if (cbed) control = cbed->GetControl();
    int maxEntries = Helpers::GetMaxAllocEntries();
    for (int i = 0; i<maxEntries; ++i )
    {
            const int pos = m_EdPosnArray[i];
            int line = -1;
            if (control && (pos != -1))
                line = control->LineFromPosition(pos);
            if (-1 not_eq line)
                MarkLine(control, line, markerId);
    }
}
// ----------------------------------------------------------------------------
void BrowseMarks::Dump()
// ----------------------------------------------------------------------------
{
    // get editor by filename in case the editor was close/opened again

    #if defined(LOGGING)
        EditorBase* eb = m_pEdMgr->GetEditor(m_filePath);
        LOGIT( _T("BT --BrowseMarks for[%p][%s]--"), eb, m_fileShortName.c_str() );
        cbEditor* cbed = 0;
        cbStyledTextCtrl* control = 0;
        //cbEditor* cbed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (eb) cbed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(eb);
        if (cbed) control = cbed->GetControl();
        LOGIT( _T("BT Array[%p] Current[%d]Last[%d]"), this, m_currIndex, m_lastIndex);
        int maxEntries = Helpers::GetMaxAllocEntries();
        for (int i = 0; i < maxEntries; ++i )
        {
                const int pos = m_EdPosnArray[i];
                if (control && (pos != -1))
                LOGIT(_T("BT Array[%p] index[%d]Line[%d]Pos[%d]"), this, i, control->LineFromPosition(pos) ,pos );
                else
                LOGIT( _T("Array[%p] index[%d]Pos[%d]"), this, i, pos );
        }
   #endif
}
