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
* $Revision$
* $Id$
* $HeadURL$
*/

#ifndef WXSITEMINFO_H
#define WXSITEMINFO_H

#include <wx/string.h>
#include <wx/bitmap.h>

/** \brief Set of possible item types */
enum wxsItemType
{
    wxsTInvalid = 0,     ///< \brief Invalid item type
    wxsTWidget,          ///< \brief Widget (childless)
    wxsTContainer,       ///< \brief Widget with children
    wxsTSizer,           ///< \brief Sizer
    wxsTSpacer,          ///< \brief Spacer
    wxsTTool             ///< \brief Tool
};

/** \brief Class containing all global information about item */
class wxsItemInfo
{
    public:

        const wxBitmap& GetIcon(int Size)
        {
            if (Size == 16)
            {
                if (!Icon16.IsOk())
                    Icon16.LoadFile(Icon16File, wxBITMAP_TYPE_PNG);

                return Icon16;
            }
            else if (Size == 32)
            {
                if (!Icon32.IsOk())
                    Icon32.LoadFile(Icon32File, wxBITMAP_TYPE_PNG);

                return Icon32;
            }

            return wxNullBitmap;
        }

        void SetIcon(int Size, const wxBitmap& Bitmap)
        {
            if (Size == 16)
                Icon16 = Bitmap;
            else if (Size == 32)
                Icon32 = Bitmap;
        }

        void SetIcon(int Size, const wxString& Filename)
        {
            if (Size == 16)
                Icon16File = Filename;
            else if (Size == 32)
                Icon32File = Filename;
        }

        wxString        ClassName;      ///< \brief Item's class name
        wxsItemType     Type;           ///< \brief Item type
        wxString        License;        ///< \brief Item's license
        wxString        Author;         ///< \brief Item's author
        wxString        Email;          ///< \brief Item's author's email
        wxString        Site;           ///< \brief Site about this item
        wxString        Category;       ///< \brief Item's category (used for grouping widgets, use _T() instead of _() because this string will be used for sorting and it will be translated manually)
        long            Priority;       ///< \brief Priority used for sorting widgets inside one group, should be in range 0..100, higher priority widgets will be at the beginning of palette
        wxString        DefaultVarName; ///< \brief Prefix for default variable name (converted to uppercase will be used as prefix for identifier)
        long            Languages;      ///< \brief Coding languages used by this item
        unsigned short  VerHi;          ///< \brief Lower number of version
        unsigned short  VerLo;          ///< \brief Higher number of version
        bool            AllowInXRC;     ///< \brief Item can be used in XRC files
        int             TreeIconId;     ///< \brief Identifier of image inside resource tree

    private:

        wxBitmap        Icon32;         ///< \brief Item's icon (32x32 pixels)
        wxString        Icon32File;     ///< \brief Item's file name (32x32 pixels)
        wxBitmap        Icon16;         ///< \brief Item's icon (16x16 pixels)
        wxString        Icon16File;     ///< \brief Item's file name (16x16 pixels)
};

#endif
