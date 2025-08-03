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

#ifndef WXSTOOL_H
#define WXSTOOL_H

#include "wxsparent.h"
#include "wxsflags.h"

using namespace wxsFlags;

/** \brief Base class for wxWidgets tools like wxTimer */
class wxsTool: public wxsParent
{
    public:

        /** \brief Default properties flags used by tools */
        static const long flTool = flVariable|flSubclass|flExtraCode;

        /** \brief Ctor */
        wxsTool(
            wxsItemResData* Data,
            const wxsItemInfo* Info,
            const wxsEventDesc* EventArray = nullptr,
            const wxsStyleSet* StyleSet = nullptr,
            long PropertiesFlags = flTool);

        /** \brief Function checking if this tool can be added to this resource
         *  \note This function is only a wrapper to OnCanAddToResource function
         */
        inline bool CanAddToResource(wxsItemResData* Data,bool ShowMessage=false) { return OnCanAddToResource(Data,ShowMessage); }

    protected:

        /** \brief Enumerating properties specific to given tool
         *
         * This function should enumerate all extra properties
         * required by tool (extra means not enumerated in base properties
         * nor id nor variable name).
         * These properties will be placed at the beginning, right after
         * there will be Variable name and identifier and at the end, all
         * required base properties.
         */
        virtual void OnEnumToolProperties(long _Flags) = 0;

        /** \brief Function which adding new items to QPP
         *
         * This function may be used to add special quick properties for
         * this item.
         *
         * All QPPChild panels will be added before additional panels
         * generic for tools.
         */
        virtual void OnAddToolQPP(wxsAdvQPP* /*QPP*/) { }

        /** \brief Checking if this tool can be added to given resource
         *
         * One can override this function to limit tool's availability.
         * Default implementation checks if we are in XRC edit mode and if this
         * item can not be used inside XRC
         *
         * \param Data data of checked resource
         * \param ShowMessage if true, this function should show message box
         *        notifying about problem when can not add this tool to resource
         */
        virtual bool OnCanAddToResource(wxsItemResData* Data,bool ShowMessage);

    private:

        /** \brief Function enumerating properties with default ones
         *
         * Function enumerating item properties. The implementation
         * does call EnumContainerProperties() and adds all default properties.
         */
        virtual void OnEnumItemProperties(long _Flags);

        /** \brief Function Adding QPPChild panels for base properties of this
         *         container.
         *
         * This function calls internally AddContainerQPP to add any additional
         * QPPChild panels.
         */
        virtual void OnAddItemQPP(wxsAdvQPP* QPP);

        /** \brief Function converting item to tool class */
        virtual wxsTool* ConvertToTool() { return this; }

        /** \brief Tools don't generate preview */
        virtual wxObject* OnBuildPreview(wxWindow* /*Parent*/, long /*_Flags*/) { return nullptr; }
};



#endif
