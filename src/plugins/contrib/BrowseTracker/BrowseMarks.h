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
#ifndef BROWSEMARKS_H
#define BROWSEMARKS_H

#include "cbeditor.h"
#include "cbstyledtextctrl.h"
#include "editormanager.h"
#include <wx/string.h>

extern int gBrowse_MarkerId;
extern int gBrowse_MarkerStyle;
extern int GetBrowseMarkerId();
extern int GetBrowseMarkerStyle();

// ----------------------------------------------------------------------------
class BrowseMarks
// ----------------------------------------------------------------------------
{
    // Contains the BrowseMarks (scintilla positions) in a circular
    // queue for an editor

    public:

        BrowseMarks(wxString fullPath);
        ~BrowseMarks();
        int         GetMarkPrevious();
        int         GetMarkNext();
        int         GetMarkCurrent();
        int         GetMark(int index);
        int         GetMarkCount();
        void        RecordMark(int pos);
        void        RecordMarksFrom(BrowseMarks& otherBrowse_Marks);
        void        CopyMarksFrom(const BrowseMarks& otherBrowse_Marks);
        void        ClearMark(int startPos, int endPos);
        void        ClearAllBrowse_Marks();
        int         FindMark(int Posn);
        bool        LineHasMarker(cbStyledTextCtrl* pControl, int line, int markerId) const;
        void        MarkRemove(cbStyledTextCtrl* pControl, int line, int markerId);
        void        MarkLine(cbStyledTextCtrl* pControl, int line, int markerId);
        void        RemoveMarkerTypes( int markerId );
        void        PlaceMarkerTypes( int markerId );
        void        RebuildBrowse_Marks(cbEditor* cbed, bool addedLines);
        wxString    GetStringOfBrowse_Marks() const;
        wxString    GetFilePath(){return m_filePath;}
        void        ImportBrowse_Marks();
        void        SetBrowseMarksStyle( int style);
        void        OnEditorEventHookIgnoreMarkerChanges( bool trueOrfalse);
        void        Dump();

    protected:
    private:
        BrowseMarks();

        EditorManager* m_pEdMgr;

        wxString    m_filePath;
        wxString    m_fileShortName;
        int         m_currIndex;    //index of current cursor posn
        int         m_lastIndex;    //insertion index
        wxArrayInt  m_EdPosnArray;
};

#endif // BROWSEMARKS_H
