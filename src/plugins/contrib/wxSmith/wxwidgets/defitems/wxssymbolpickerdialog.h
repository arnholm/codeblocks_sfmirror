/** \file wxssymbolpickerdialog.h
*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2008 Ron Collins
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

#ifndef WXSSYMBOLPICKERDIALOG_H
#define WXSSYMBOLPICKERDIALOG_H

#include "../wxstool.h"

class wxsSymbolPickerDialog: public wxsTool
{
    public:

        wxsSymbolPickerDialog(wxsItemResData* Data);

    private:

        virtual void OnBuildCreatingCode();
        virtual void OnEnumToolProperties(long _Flags);

        wxString m_Message;
        wxString m_DefaultDir;
        wxString m_DefaultFile;
        wxString m_Wildcard;
};

#endif      //  WXSSYMBOLPICKERDIALOG_H
