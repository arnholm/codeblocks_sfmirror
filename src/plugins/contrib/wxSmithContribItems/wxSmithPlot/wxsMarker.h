/*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2006-2007  Bartlomiej Swiecki
*
* wxSmith is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* wxSmith is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmith. If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifndef WXSMARKER_H
#define WXSMARKER_H

#include <iostream>

#include <wx/stattext.h>
#include <wxwidgets/wxswidget.h>
#include <mathplot.h>

class wxsMarker: public wxsWidget
{
    public:

        wxsMarker(wxsItemResData* Data);

    private:

        virtual void      OnBuildCreatingCode();
        virtual wxObject* OnBuildPreview(wxWindow* Parent, long Flags);
        virtual void      OnBuildDeclarationsCode();
        virtual void      OnEnumWidgetProperties(long Flags);

        wxString      mLabel;
        wxString      mXpos, mYpos;
        wxsColourData mPenColour;   // color to draw
        wxsFontData   mPenFont;     // for drawing the text
};

#endif // WXSMARKER_H
