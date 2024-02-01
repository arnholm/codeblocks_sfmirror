#ifndef PARSERCOMMON_H_INCLUDED
#define PARSERCOMMON_H_INCLUDED

#include "wx/string.h"
#include <list>
#include <set>


typedef std::set<wxString>  StringSet;
typedef std::list<wxString> StringList;

/** specify the sort order of the symbol tree nodes */
enum BrowserSortType
{
    bstAlphabet = 0, /// alphabetical
    bstKind,         /// class, function, macros
    bstScope,        /// public, protected, private
    bstLine,         /// code like order
    bstNone
};
/** specify the scope of the shown symbols */
enum BrowserDisplayFilter
{
    bdfFile = 0,  /// display symbols of current file
    bdfProject,   /// display symbols of current project
    bdfWorkspace, /// display symbols of current workspace //unsupported in clangd_client //(ph 2024/01/13)
    bdfEverything /// display every symbols
};

// ----------------------------------------------------------------------------
namespace ParserCommon
// ----------------------------------------------------------------------------
{
    /** the enum type of the file type */
    enum EFileType
    {
        ftHeader,
        ftSource,
        ftOther
    };

    /** return a file type, which can be either header files or implementation files or other files
     *  @param filename the input file name
     *  @param force_refresh read the user's option of file extension to classify the file type
     */
    EFileType FileType(const wxString& filename, bool force_refresh = false);
}// namespace ParserCommon
// ----------------------------------------------------------------------------
/** Setting of the Parser, some of them will be passed down to ParserThreadOptions */
struct ParserOptions
// ----------------------------------------------------------------------------
{
    ParserOptions():
        followLocalIncludes(true),
        followGlobalIncludes(true),
        caseSensitive(true),
        wantPreprocessor(true),
        useSmartSense(true),
        whileTyping(true),
        parseComplexMacros(true),
        platformCheck(true),
        logClangdClientCheck(false),
        logClangdServerCheck(false),
        logPluginInfoCheck(false),
        logPluginDebugCheck(false),
        lspMsgsFocusOnSaveCheck(false),
        lspMsgsClearOnSaveCheck(false),
        LLVM_MasterPath(""),
        storeDocumentation(true)
    {}

    bool followLocalIncludes;  /// parse XXX.h in directive #include "XXX.h"
    bool followGlobalIncludes; /// parse XXX.h in directive #include <XXX.h>
    bool caseSensitive;        /// case sensitive in MarkItemsByAI
    bool wantPreprocessor;     /// handle preprocessor directive in Tokenizer class
    bool useSmartSense;        /// use real AI(scope sequence match) or not(plain text match)
    bool whileTyping;          /// reparse the active editor while editing
    bool parseComplexMacros;   /// this will let the Tokenizer to recursive expand macros
    bool platformCheck;        /// this will check for the platform of the project/target when adding include folders to the parser
    bool logClangdClientCheck; /// this will check for user enabled clangd client logging
    bool logClangdServerCheck; /// this will check for user enabled clangd server logging
    bool logPluginInfoCheck;   /// this will check for user enabled plugin info logging
    bool logPluginDebugCheck;  /// this will check for user enabled plugin debug logging
    bool lspMsgsFocusOnSaveCheck; /// this will check for user enabled Focus LSP messages tab on save text
    bool lspMsgsClearOnSaveCheck; /// this will check for user enabled LSP messages tab clear on save text
    wxString LLVM_MasterPath;  /// Path to LLVM install directory
    bool storeDocumentation;   /// should tokenizer detect and store doxygen documentation?

};
// ----------------------------------------------------------------------------
/** Options for the symbol browser, this specify how the symbol browser will shown */
struct BrowserOptions
// ----------------------------------------------------------------------------
{
    BrowserOptions():
        showInheritance(false),
        expandNS(false),
        treeMembers(true),
        displayFilter(bdfFile),
        sortType(bstKind)
    {}

    /** whether the base class or derive class information is shown as a child node
     * default: false
     */
    bool                 showInheritance;

    /** whether a namespaces node is auto-expand
     * auto-expand means the child of the namespace is automatically added.
     * default: false, so the user has to click on the '+' icon to expand the namespace, and
     * at this time, the child will be added.
     */
    bool                 expandNS;

    /** show members in the bottom tree. default: true */
    bool                 treeMembers;

    /** token filter option
     *  @see  BrowserDisplayFilter for details
     *  default: bdfFile
     */
    BrowserDisplayFilter displayFilter;

    /** token sort option in the tree
     *  default: bstKind
     */
    BrowserSortType      sortType;
};


#endif // PARSERCOMMON_H_INCLUDED
