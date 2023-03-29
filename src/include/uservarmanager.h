/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef USER_VARIABLE_MANAGER_H
#define USER_VARIABLE_MANAGER_H

#include "settings.h"
#include "manager.h"
#include "cbexception.h"

#include <unordered_map>
#include <set>
#include <wx/regex.h>
#include <algorithm>

#include <wxstringhash.h>       // For wx < 3.1 we need this.

#ifndef CB_PRECOMP
#include "globals.h"
#endif

/** \brief Virtual class representing UI for UserVariableManager
 */
class UserVarManagerUI
{
public:
    virtual ~UserVarManagerUI() {};

    /** \brief Show an information window to the user
     *
     * This window
     * \param title const wxString& Title of the information window
     * \param msg const wxString& Content of the info window
     */
    virtual void DisplayInfoWindow(const wxString &title,const wxString &msg) = 0;

    /** \brief Open the variable editor window and if the not exist create the variables from the set
     *
     * \param var Variables to create
     */
    virtual void OpenEditWindow(const std::set<wxString> &var = std::set<wxString>()) = 0;

    /** \brief Open Dialog that asks the user for a variable
     *
     * \param parent wxWindow* Parent window
     * \param old const wxString& Old variable to display as selected
     * \return virtual wxString Global variable the user has selected
     *
     */
    virtual wxString GetVariable(wxWindow* parent, const wxString &old) = 0;
};

/** \brief Class to model user variable member
 */
class UserVariableMember
{
private:
    wxString m_Name;
    wxString m_Value;
    wxString m_Desc;

public:
    UserVariableMember(const wxString& name) : m_Name(name)
    {
    }

    UserVariableMember(const wxString& name, const wxString& value ) : m_Name(name), m_Value(value)
    {
    }

    bool IsValid() const
    {
        return !m_Value.IsEmpty();
    }

    void SetValue(const wxString& value)
    {
        m_Value = value;
    }

    wxString GetValue() const
    {
        return m_Value;
    }

    wxString GetName() const
    {
        return m_Name;
    }

    void SetDesc(const wxString& desc)
    {
        m_Desc = desc;
    }

    wxString GetDesc() const
    {
        return m_Desc;
    }
};

/** \brief Default member names
 *
 * This names are used to store default members in the settings XML
 *
 */
namespace UserVariableManagerConsts
{
static const wxString cBase     = wxString (_T("base"));        //!> Base variable value
static const wxString cInclude  = wxString (_T("include"));
static const wxString cLib      = wxString (_T("lib"));
static const wxString cObj      = wxString (_T("obj"));
static const wxString cBin      = wxString (_T("bin"));
static const wxString cCflags   = wxString (_T("cflags"));
static const wxString cLflags   = wxString (_T("lflags"));
static const std::vector<wxString> cBuiltinMembers = {cBase, cInclude, cLib, cObj, cBin, cCflags, cLflags};
static const wxString cSets     (_T("/sets/"));
static const wxString cDir      (_T("dir"));

static const wxString defaultSetName      (_T("default"));

}

class UserVariable
{
private:

    typedef std::unordered_map<wxString, UserVariableMember> UserVarMemberMap;

    UserVarMemberMap m_Values;
    wxString m_Name;

public:

    UserVariable(const wxString& name) : m_Name(name)
    {
    }

    UserVariable(const wxString& name, const UserVariable& other) : m_Name(name)
    {
        m_Values = other.m_Values;
    }

    /** \brief Check if the variable is valid
     *
     * A user variable is valid if it has an base value
     * \return true if base member is defined and valid
     */
    bool IsValid() const
    {
        return m_Values.find(UserVariableManagerConsts::cBase) != m_Values.end() && m_Values.at(UserVariableManagerConsts::cBase).IsValid();
    }


    /** \brief Set the value of a member. If the member does not exist, create it
     *
     * \param member member name
     * \param value member value
     */
    void SetValue(const wxString member, const wxString& value)
    {
        UserVarMemberMap::iterator itr = m_Values.find(member);

        if (value.IsEmpty() && itr != m_Values.end())
        {
            m_Values.erase(itr);
            return;
        }
        if (itr == m_Values.end())
            m_Values.emplace(member, UserVariableMember(member, value));
        else
            itr->second.SetValue(value);
    }

    /** \brief Get value of member variable. Returns true if the value is valid
     *
     *  If \param member is empty, return base value
     *
     * \param[in] member Name of the member variable
     * \param[out] value wxString&
     * \param[in] doNotAutoReplace If true, and the member is not defined, but a base exists, add the member name to base with path seperator
     * \return true if member exists and value is valid
     */
    bool GetValue(wxString member, wxString& value, bool doNotAutoReplace = false) const
    {
        if (member.IsEmpty())
            member = UserVariableManagerConsts::cBase;

        UserVarMemberMap::const_iterator memberItr = m_Values.find(member);
        if (member.IsSameAs(UserVariableManagerConsts::cInclude) || member.IsSameAs(UserVariableManagerConsts::cLib) || member.IsSameAs(UserVariableManagerConsts::cObj) || member.IsSameAs(UserVariableManagerConsts::cBin))
        {
            if (memberItr == m_Values.end())
            {
                UserVarMemberMap::const_iterator baseItr = m_Values.find(UserVariableManagerConsts::cBase);
                if (baseItr == m_Values.end())
                    return false;
                const wxString base = baseItr->second.GetValue();
                if (!doNotAutoReplace)
                    value = base + _T('/') + member;
                return true;
            }
            value = memberItr->second.GetValue();
            return true;
        }
        if (memberItr == m_Values.end())
            return false; // "(invalid variable " + member + ")";
        value = memberItr->second.GetValue();
        return true;
    }

    /** \brief Return true, if the member exists
     *
     * \param member name of member to check
     * \return true if member exists
     */
    bool HasMember(const wxString& member) const
    {
        wxString tmp;
        return GetValue(member, tmp);
    }


    /** \brief Return value of 'base' member, empty string if invalid
     *
     * \return wxString
     *
     */
    wxString GetBase() const
    {
        if (m_Values.find(UserVariableManagerConsts::cBase) == m_Values.end())
            return wxString(); //"(invalid variable " + m_Name + ")";
        return m_Values.at(UserVariableManagerConsts::cBase).GetValue();
    }

    /** \brief Get name of variable
     *
     * \return wxString name of variable
     */
    wxString GetName() const
    {
        return m_Name;
    }

    /** \brief Get member variable, if it does not exist, create it
     *
     * \param name name of member variable
     * \return member variable. If it does not exists, a new member is created
     */
    UserVariableMember& GetMember(const wxString& name)
    {
        if (m_Values.find(name) == m_Values.end())
            m_Values.emplace(name, UserVariableMember(name));

        return m_Values.at(name);
    }

    /** \brief Delete member, do nothing if it does not exists
     *
     * \param name name of member variable to delete
     */
    void RemoveMember(const wxString& name)
    {
        m_Values.erase(name);
    }

    /** \brief Return all member names
     */
    std::vector<wxString> GetMembers() const
    {
        std::vector<wxString> members;

        for (std::unordered_map<wxString, UserVariableMember>::const_iterator itr = m_Values.cbegin(); itr != m_Values.cend(); ++itr)
        {
            if (!std::binary_search(members.begin(), members.end(), itr->first))
                members.push_back(itr->first);
        }
        return members;
    }
};

typedef std::unordered_map<wxString, UserVariable>  VariableMap;        /**< Type to save all variables */
typedef std::unordered_map<wxString, VariableMap>   VariableSetMap;     /**< Type to save all sets */

/** \brief Singelton class that manages User Variables
*
* # Generic information about User Variables
* User Variables (UV) (aka global variables, aka global compiler variables ) are variables that get replaced automatically
* in  macros with the syntax  `$(#variable.member)`. They are managed globally by the UserVariableManger and
* replaced by an user defined value.
* UV are structured hierarchically:
*
*      Set 1
*      |------ variable 1
*      |           |-------- Base
*      |           |-------- Member 1
*      |           |-------- Member 2
*      |-------Variable 2
*      |           |-------- Base
*      |           |-------- Member 1
*      |           |-------- Member 2
*      Set 2
*      |------ Variable  1
*      |           |-------- Base
*      |           |-------- Member 1
*      |           |-------- Member 2
*
* \li Only one Set can be active at the same time.
* \li Variable names in one set have to be unique.
* \li Member names in one variable have to be unique
* \li Each variable has a \a base value, that has to be defined and is accessed with syntax `$(#variable)`
* \li Each variable has some default members that are generated automatically if not defined. This defaults are defined in \ref UserVariableManagerConsts.cBuiltinMembers
* If for example the member default `include` of the variable `test` is not defined then  `$(#test.include)` gets automatically replaced to `basevalue/include`
*
* UV can be defined in the GUI or via command line `-D variable.member=value`
*
* # Implementation details
*
*
 */
class DLLIMPORT UserVariableManager : public Mgr<UserVariableManager>
{
    friend class Manager;
    friend class Mgr<UserVariableManager>;
    friend class MacrosManager;

    ConfigManager * m_CfgMan;
    wxString        m_ActiveSet;                //!< Store to current active set name
    std::set<wxString>   m_Preempted;           //!< A list with all found but not existing variables that occurred during project loading.
    std::unique_ptr<UserVarManagerUI> m_ui;

    VariableSetMap m_VariableSetMap;            //!< Default variable set map, loaded from global settings
    bool setNameOverwrittenInParameter;         //!< True if the active set is overridden via command line argument. If true do not store active set on closing codeblocks
    VariableSetMap m_ParameterVariableSetMap;   //!< Parameter map loaded from command line

    /** \brief Get the value of the member from a variable from a given set map
     *
     * \param[in] setMap  a map of sets in which the member variable should be searched
     * \param[out] value If found the value of the member variable. Only valid if return value is true
     * \param[in] setName Set name
     * \param[in] varName variable name
     * \param[in] memberName member name
     * \return true if member is found and \ref value is valid.
     */
    bool GetMemberValue(const VariableSetMap& setMap, wxString& value, const wxString& setName, const wxString& varName, const wxString& memberName) const;


    /** \brief Find the value of the member from a variable in the current active set
     *
     * This function searches automatically for a member variable first in the command line cache, if there not found,
     * in the global user variables stored by the global settings
     *
     * \param[out] value Valid member variable value if return value is true
     * \param[in] varName Name of the variable
     * \param[in] memberName Name of the member
     * \return If true, member found and \ref value is valid
     */
    bool GetMemberValue(wxString& value, const wxString& varName, const wxString& memberName) const;


    /** \brief Returns true if the member variable has a valid value in the given set map and active set
     *
     * \param[in] setMap Set map in which the variable should be searched
     * \param[in] varName Variable name
     * \param[in] memberName Member name
     * \return return true if member has valid value
     */
    bool Exists(const VariableSetMap& setMap, const wxString& varName, const wxString& memberName) const;


    /** \brief Returns true if the member variable has a valid value in the active set and provided set map
     *
     * It is only searched in the currently active set.
     * The variable has the syntax `$(#variable.member)` and is automatically parsed
     *
     * \param[in] setMap
     * \param[in] variable
     * \return true if the \ref variable has correct syntax and member is found
     */
    bool Exists(const VariableSetMap& setMap, const wxString& variable) const;


    /** \brief Split string in variable name and member name
     *
     * Split `$(#variable.member)` in variable name and member name
     * \param[in] variable Input string to parse
     * \param[out] varName
     * \param[out] memberName
     * \return true if syntax correct and \ref varName and \ref memberName are valid
     */
    bool ParseVariableName(const wxString& variable, wxString& varName, wxString& memberName) const;


public:
    UserVariableManager();
    ~UserVariableManager();

    void SetUI(std::unique_ptr<UserVarManagerUI> ui);

    /** \brief Return value of variable with syntax `$(#variable.member)` from the current active set
     *
     * return value of the variable. The variable is first searched in the command line map, if there not
     * found the global map is used. Only the active set is searched.
     * If the variable is not found and \ref errorMessages != null some error message is logged in errorMessages
     * \param[in] variable string to replace with syntax `$(#variable.member)`
     * \param[out] errorMessages
     * \return The result string or an empty string
     */
    wxString Replace(const wxString& variable, std::vector<wxString>* errorMessages = nullptr);

    /** \brief Ask user for a global variable
     *
     * This function opens a UI for asking user to input a variale
     * \param parent parent window
     * \param old old value to display in the UI
     * \return empty string on cancel, or the
     */
    wxString GetVariable(wxWindow *parent, const wxString &old);

    /** \brief If the variable does not exist, cache it in a list
     *
     * This function is used to check if a variable exists during a loading process and if not cache it.
     * At the ending of this loading process \ref Arrogate() is called. And cached non existent variables
     * are asked from the user.
     * \param variable Variable to check and if not existent cache it
     */
    void Preempt(const wxString& variable);

    /** \brief Ask user for all non existent variables collected with \ref Preempt(const wxString& variable)
     */
    void Arrogate();


    /** \brief Check if the variable exists and is valid (at least the base member is set)
     *
     * \param variable Variable to check
     * \return true if the variable exists
     */
    bool Exists(const wxString& variable) const;


    /** \brief Check if this variable is overridden by command line parameter
     *
     * \param varName variable name to check
     * \param member member to check
     * \return true if this variable.member is overridden by command line -D
     */
    bool IsOverridden(const wxString& varName, const wxString& member) const;


    /** \brief Check if this variable is overridden by command line parameter
     *
     * \param variable variable with syntax `$(#variable)`
     * \return true if this variable is overridden by command line -D
     */
    bool IsOverridden(const wxString& variable) const;

    /** \brief Open configuration UI
     */
    void Configure();

    void Migrate();

    UserVariableManager& operator=(const UserVariableManager&) = delete;
    UserVariableManager(const UserVariableManager&) = delete;


    /** \brief Get all member names of the given set and name
     *
     * \param setName Set name to search variable
     * \param varName Variable name
     * \return empty vector if the set/variable does not exists or member names if successful
     */
    std::vector<wxString> GetMemberNames(const wxString& setName, const wxString& varName) const;

    /** \brief Get all variables in a given set
     *
     * \param setName name of set
     * \return empty vector if set does not exists, variable names in set othervise
     */
    std::vector<wxString> GetVariableNames(const wxString& setName) const;


    /** \brief Get all set names
     *
     * \return vector of set names
     */
    std::vector<wxString> GetVariableSetNames() const;


    /** \brief Return copy of all variables
     */
    VariableSetMap GetVariableMap();


    /** \brief Update internal set/variable map with the given map
     *
     * \param varMap Map to be used
     */
    void UpdateFromVariableMap(VariableSetMap varMap);

    /** \brief Create a new variable set
     *
     * \param setName name of the new set to create
     */
    void CreateVariableSet(const wxString& setName);

    /** \brief Get value of member variable. First search in program arguments, then in configuration file
     *
     * \param setName set name
     * \param varName variable name
     * \param memberName member name
     * \return member variable value
     */
    wxString GetMemberValue(const wxString& setName, const wxString& varName, const wxString& memberName) const;


    /** \brief Get active set name
     *
     * \return name of active set
     */
    wxString GetActiveSetName() const
    {
        return m_ActiveSet;
    };

    /** \brief Set active set
     *
     * \param setName valid set name
     * \return true if the set name is valid, false otherwise
     */
    bool SetActiveSetName(const wxString& setName);

    /** \brief Reload global variables from settings file
     */
    void Reload();

    /** \brief Save global variables to settings file
     */
    void Save();

    /** \brief Parse command line arguments to override variables from settings file
     *
     * supported parameters are:
     * Parse command line
     * '-D' set a variabel for ex.
     * "-D set.variable.member=value" or "-D variable.member=value"
     *
     * '-S' set active set
     * "-S setName"
     *
     * \param parser wxWidgets command line parser
     */
    void ParseCommandLine(wxCmdLineParser& parser);

    /** \brief Set variable value in current set
     *
     * \param varName variable name
     * \param memberName member name
     * \param value value to set variable to
     * \param createIfNotPresent if true and the variable does not exists, then the variable is created. Default false.
     * \return bool true if the variable is set
     */
    bool SetVariable(const wxString& varName, const wxString& memberName, const wxString& value, bool createIfNotPresent = false);

    /** \brief Set variable value in current set
     *
     * \param setName set name
     * \param varName variable name
     * \param memberName member name
     * \param value value to set variable to
     * \param createIfNotPresent if true and the variable does not exists, then the variable is created.
     *                            If the set does not exists it is not created, also if this parameter is set to true.
     *                            Default false.
     * \return bool true if the variable is set
     */
    bool SetVariable(const wxString& setName, const wxString& varName, const wxString& memberName, const wxString& value, bool createIfNotPresent = false);
};

#endif // USER_VARIABLE_MANAGER_H
