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

#ifndef WXSPROJECT_H
#define WXSPROJECT_H

#include <cbproject.h>
#include <tinyxml.h>
#include "wxsresourcetree.h"

class wxSmith;
class wxsResource;
class wxsGUI;

/** \brief This class integrates current project with wxsmith. */
class wxsProject
{
    public:

        /** \brief Ctor */
        wxsProject(cbProject* Project);

        /** \brief Dctor */
        ~wxsProject();

        /** \brief Getting C::B project */
        inline cbProject* GetCBProject() { return m_Project; }

        /** \brief Reading configuration from given xml node and creating resource classes
         * \note To add backward compatibility, if there's no valid configuration inside
         *       cbp file, this function looks for outdated wxsmith.cfg file inside internal
         *       directory and loads configuration from that file
         */
        void ReadConfiguration(TiXmlElement* element);

        /** \brief Storing configuration to given xml node */
        void WriteConfiguration(TiXmlElement* element);

        /** \brief Searching for resource with given name */
        wxsResource* FindResource(const wxString& Name);

        /** \brief Displaying configuration dialog */
        void Configure();

        /** \brief Creating extra configuration panel in project options */
        cbConfigurationPanel* GetProjectConfigurationPanel(wxWindow* parent);

        /** \brief Function returning main project path */
        wxString GetProjectPath();

        /** \brief Adding new resource
         *
         * This function will modify CBP project's modify state
         */
        bool AddResource(wxsResource* Resource);

        /** \brief Getting number of resources in this project */
        inline int GetResourcesCount() { return (int)m_Resources.Count(); }

        /** \brief Getting resource from index */
        inline wxsResource* GetResource(int Index) { return ((Index>=0)&(Index<GetResourcesCount()))?m_Resources[Index]:nullptr; }

        /** \brief Removing resource from project */
        bool DelResource(wxsResource* Resource);

        /** \brief Function checking if given file can be opened from this project
         * \param FileName name of file used by this resource
         *
         * This function is used by MIME plugin to check if specified file
         * is used inside any of resources inside this project. If true, editor
         * can be opened for this resource.
         */
        bool CanOpenEditor(const wxString& FileName);

        /** \brief Function trying to open editor for project with given file
         * \param FileName name of file used by this resource
         *
         * This function should check if resources are using given file
         * and if yes, it should open valid editor for it.
         */
        bool TryOpenEditor(const wxString& FileName);

        /** \brief Getting current wxsGUI item */
        inline wxsGUI* GetGUI() { return m_GUI; }

        /** \brief Changing GUI item
         *
         * wxsProject class become owner of new wxsGUI item and will
         * delete if if it won't be longer used (when will change to
         * other wxsGUI or while destructing wxsProject class).
         */
        void SetGUI(wxsGUI* NewGUI);

        /** \brief notifying about changes in project */
        void NotifyChange();

        /** \brief Checking if project has been modified during load */
        inline bool GetWasModifiedDuringLoad() { return m_WasModifiedDuringLoad; }

        /** \brief Function updating project's name
         *
         * Currently it only updates name of project in resource browser
         */
        void UpdateName();

        /** \brief Try to recover wxs file */
        bool RecoverWxsFile( const wxString& ResourceDescription );

    private:

        WX_DEFINE_ARRAY(wxsResource*,ResourcesT);
        WX_DECLARE_STRING_HASH_MAP(wxsResourceItemId,ResBrowserIdsT);
        typedef ResBrowserIdsT::iterator ResBrowserIdsI;

        wxString m_ProjectPath;             ///< \brief Base Directory of C::B project (where project file is stored)
        cbProject* m_Project;               ///< \brief Project associated with project
        wxsResourceItemId m_TreeItem;       ///< \brief Tree item where project's resources are kept
        ResourcesT m_Resources;             ///< \brief Array of resources
        ResBrowserIdsT m_ResBrowserIds;     ///< \brief wxTreeItemId-s for resource types
        wxsGUI* m_GUI;                      ///< \brief Main GUI item
        TiXmlElement m_UnknownConfig;
        TiXmlElement m_UnknownResources;
        bool m_WasModifiedDuringLoad;       ///< \brief Set to true if project had to be converted to some newer format during load

        /** \brief Getting tree item id in resource browser associated with given resource type */
        wxsResourceItemId GetResourceTypeTreeId(const wxString& Name);

        friend class wxsResource;
};

#endif
