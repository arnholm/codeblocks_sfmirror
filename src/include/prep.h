/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#if ( !defined(PREP_H) && defined(__cplusplus) )
#define PREP_H

#include <stdint.h>

#include <wx/defs.h> // wxIntPtr
#ifndef wxMAJOR_VERSION
    #include <wx/version.h>
#endif

// The Code::Blocks doesn't officially support wxWidgets versions lower than 3.0.0 on Unix and 3.1.3
// on Windows/macOS. If you want to use unsupported version you could comment the checks below, but
// keep in mind that any bug you report with these versions might be ignored. Patches to fix the
// build with such versions might also be ignored.
#if (defined(__WXMSW__) || defined(__WXOSX_COCOA__))
    #if !wxCHECK_VERSION(3, 1, 3)
        #pragma message("Unsupported wxWidgets Version: " wxVERSION_NUM_DOT_STRING)
        #error "Unsupported wxWidgets Version"
    #endif
#else
    #if !wxCHECK_VERSION(3, 0, 0)
        #pragma message("Unsupported wxWidgets Version: " wxVERSION_NUM_DOT_STRING)
        #error "Unsupported wxWidgets Version"
    #endif
#endif

/*  ---------------------------------------------------------------------------------------------------------
    Version<major, minor, revision>::eval
        Integer compile-time constant that represents  a major.minor.revision style version number.
        This is the most convenient and efficient way to check for API-version dependent features,
        as it can be done as easily as:  if(foobar_version >= Version<1,2>::eval)
        The range of possible version numbers is [Version<0,0,0> , Version<63,1023,32767>]

    wxMinimumVersion<...>::eval, wxExactVersion<...>::eval
        Boolean values that are true if the compiled wxWidgets version is at least (exactly)
        the version given as parameters.

    Example:
        if(!wxMinimumVersion<2.8>::eval && (foo_version < Version<1.2>::eval))
            ErrorMessage("This feature is only supported under wxWidgets 2.8, or with Foo Component 1.2 or higher.");
*/
template <int major, int minor = 0, int revision = 0> struct Version { enum { eval = (major<<25) + (minor<<15) + revision }; };
inline void Version2MMR(int v, int& major, int& minor, int& revision) { major = v>>25; minor = (v>>15) & ((1<<10) -1); revision = v & ((1<<15) -1); }

template <int major, int minor, int rel = 0> struct wxMinimumVersion { enum { eval = ((unsigned int)  Version<wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER>::eval >= (unsigned int) Version<major, minor, rel>::eval) }; };
template <int major, int minor, int rel = 0> struct wxExactVersion { enum { eval = ((unsigned int) Version<wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER>::eval == (unsigned int) Version<major, minor, rel>::eval) }; };


/*  ---------------------------------------------------------------------------------------------------------
    Conditional typedef, works the same way as the C++ ternary operator for lvalues does

    You want to do something like:
        typedef (condition ? true_type : false_type) my_type;

    This can for example be used to define a type as
        FILE under Unix / HANDLE under Windows
        int under wxWidgets 2.6 / size_t under wxWidgets 2.8
        STL iterator / wxContainer iterator / pointer ...
    and not having to worry about what it really is, without giving up type safety and without
    the nasty side effects that a #define might have.

    Example:
        typedef TernaryCondTypedef<wxMinimumVersion<2,5>::eval, wxTreeItemIdValue, long int>::eval tree_cookie_t;
*/
template <bool cond, class true_t, class false_t> struct TernaryCondTypedef { typedef true_t eval; };
template <class true_t, class false_t> struct TernaryCondTypedef<false, true_t, false_t> { typedef false_t eval; };



/*  ---------------------------------------------------------------------------------------------------------
    Size of an array (if the compiler can determine it at compile time)
    Write:
        int widths[] = {5, 3, 8};
        myListControl->SetWidths(widths, array_size(widths));
    instead of:
        int widths[] = {5, 3, 8};
        myListControl->SetWidths(widths, 4); // oh crap, why does this crash?
*/
template <typename T> unsigned int array_size(const T& array) { enum {result = sizeof(array) / sizeof(array[0])}; return result; }



/*  ---------------------------------------------------------------------------------------------------------
    Delete a pointer and (semi-atomically) set it to zero.
    In class destructors, please continue using the normal C++ delete operator (it is unnecessary overhead
    to set a pointer to zero in the destructor, as it can never be used again).
    In _all_ other cases, use Delete(), which prevents accidential double-deletes.
*/
template<typename T>inline void Delete(T*& p){delete p; p = nullptr;}
template<typename T>inline void DeleteArray(T*& p){delete[] p; p = nullptr;}



/*  ---------------------------------------------------------------------------------------------------------
    platform::id
        Value of type platform::identifier describing the target platform

    platform::windows, platform::macosx, platform::Linux
    platform::freebsd, platform::netbsd, platform::openbsd
    platform::darwin,  platform::solaris, platform::Unix
        Boolean value that evaluates to true if the target platform is the same as the variable's name, false otherwise.
        Using the platform booleans is equivalent to using platform::id, but results in nicer code.

    platform::unicode
        Boolean value that evaluates to true if this application was built in Unicode mode. Of course this does not
        tell anything about the host system's actual capabilities.

    platform::gtk
    platform::carbon
    platform::cocoa
        Boolean values showing the presence of very specific toolkits. Use only for workarounds to specific problems with these.

    platform::bits
        Size of pointer in bits as a measure of CPU architecture (32 or 64 bits).

    platform::gcc
        The gcc version number as Version<...> if gcc was used to build the program, zero otherwise.
*/
namespace platform
{
    enum identifier
    {
        platform_unknown,
        platform_windows,
        platform_linux,
        platform_freebsd,
        platform_netbsd,
        platform_openbsd,
        platform_darwin,
        platform_solaris,
        platform_macosx
    };

    // unfortunately we still need to use the preprocessor here...
    #if ( wxUSE_UNICODE )
    const bool unicode = true;
    #else
    const bool unicode = false;
    #endif

    #if   defined ( __WIN32__ ) || defined ( _WIN64 )
    const identifier id = platform_windows;
    #elif defined ( __WXMAC__ )  || defined ( __WXCOCOA__ )
    const identifier id = platform_macosx;
    #elif defined ( __linux__ )  || defined ( LINUX )
    const identifier id = platform_linux;
    #elif defined ( FREEBSD )    || defined ( __FREEBSD__ )
    const identifier id = platform_freebsd;
    #elif defined ( NETBSD )     || defined ( __NETBSD__ )
    const identifier id = platform_netbsd;
    #elif defined ( OPENBSD )    || defined ( __OPENBSD__ )
    const identifier id = platform_openbsd;
    #elif defined ( DARWIN )     || defined ( __APPLE__ )
    const identifier id = platform_darwin;
    #elif defined(sun) || defined(__sun)
    const identifier id = platform_solaris;
    #else
    const identifier id = platform_unknown;
    #endif

    #if   defined ( __WXGTK__ )
    const bool gtk = true;
    #else
    const bool gtk = false;
    #endif

    #if   defined ( __WXMAC__ )
    const bool carbon = true;
    #else
    const bool carbon = false;
    #endif

    #if   defined ( __WXCOCOA__ )
    const bool cocoa = true;
    #else
    const bool cocoa = false;
    #endif

    const bool windows = (id == platform_windows);
    const bool macosx  = (id == platform_macosx);
    const bool Linux   = (id == platform_linux);
    const bool freebsd = (id == platform_freebsd);
    const bool netbsd  = (id == platform_netbsd);
    const bool openbsd = (id == platform_openbsd);
    const bool darwin  = (id == platform_darwin);
    const bool solaris = (id == platform_solaris);
    const bool Unix    = (Linux | freebsd | netbsd | openbsd | darwin | solaris);

    const int bits = 8*sizeof(void*);

    // Function and parameter attributes
    // ----------------------------------
    //
    // These parameters possibly allow more fine-grained optimization and better diagnostics.
    // They are implemented for the GCC compiler family and 'quiet' for all others
    //
    // IMPORTANT: Do not lie to the compiler when marking functions pure or const, or you will cause great evil.
    //
    // a) Optimization hints:
    //
    // cb_pure_function        Function has no observable effects other than the return value
    //                         and the return value depends only on parameters and global variables
    //                         ALSO: function does not throw (makes sense with the above requirement).
    //
    // cb_const_function       Function has no observable effects other than the return value
    //                         and the return value depends only on parameters.
    //                         ALSO: No external memory, including memory referenced by parameters, may be touched.
    //                         ALSO: function does not throw (makes sense with the above requirement).
    //
    // cb_force_inline         What the name says. This is the strongest available inline hint (the compiler may still ignore it).
    //
    //
    // b) Usage hints:
    //
    // cb_must_consume_result  The return value of a function must be consumed (usually because of taking ownership), i.e.
    //                         ObjType* result = function();  ---> valid
    //                         function();                    ---> will raise a warning
    //
    // cb_deprecated_function  The function is deprecated and may be removed in a future revision of the API.
    //                         New code should not call the function. Doing so will work, but will raise a warning.
    //
    // cb_optional             No warning will be raised if the parameter is not used. Identical to "unused",
    //                         but the wording sounds more like "you may omit using this" rather than "we are not using this"
    //                         Used for interfaces or base classes to convey the message that a (generally useful) parameter is passed,
	//                         but it is allowable to ignore the parameter (and maybe a base class implementation does just that).
    //
    // cb_unused               No warning will be raised if the parameter is not used.
    //                         Use this if you want to convey that you are aware of a parameter but you are intentionally not using it.

    #if defined(__GNUC__)
        const int gcc = Version<__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__>::eval;
        #define cb_pure_function       __attribute__ ((__pure__,  __nothrow__))
        #define cb_const_function      __attribute__ ((__const__, __nothrow__))
        #define cb_force_inline        __attribute__ ((__always_inline__))
        #define cb_must_consume_result __attribute__ ((__warn_unused_result__))
        #define cb_deprecated_function __attribute__ ((__deprecated__))
        #define cb_unused              __attribute__ ((__unused__))
    #else
        const int gcc = 0;
        #define cb_pure_function
        #define cb_const_function
        #define cb_force_inline
        #define cb_must_consume_result
        #define cb_deprecated_function
        #define cb_unused
        #define POISON(message)
    #endif

    #define cb_optional cb_unused
}



/*  ---------------------------------------------------------------------------------------------------------
    Nobody except Yiannis touches these. You don't need to know, you don't want to know.
*/
namespace sdk
{
    const int version             = Version<1>::eval;
    const int buildsystem_version = Version<1>::eval;
    const int plugin_api_version  = Version<1,11,10>::eval;
}



/*  ---------------------------------------------------------------------------------------------------------
    The compatibility namespace is intended for workarounds that try to cope with incompatibilities in different
    wxWidgets versions. Since these often involve missing functions or constants, #ifdef is explicitly allowed
    and not frowned upon in this namespace.

    wxHideReadonly
        use in file selector dialog to keep wxWidgets 2.4/2.6 users happy (this flag is important for normal operation!),
        without breaking the 2.8 build.
*/
namespace compatibility
{
    #if defined(WXWIN_COMPATIBILITY_2_4) && defined(wxHIDE_READONLY)
        const int wxHideReadonly = wxHIDE_READONLY;
    #else
        const int wxHideReadonly = 0;
    #endif
}



/*  ---------------------------------------------------------------------------------------------------------
    Utility function for an incrementing (unique per instance) unsigned integer ID.
        - ID is unsigned, starting at zero (best choice for many cases, e.g. for use as array index)
        - if it _must_ start with 1 for some odd reason, simply add one extra call to GetID()<YourClass>
        - use GetID() for an application-wide unique integer ID (alias for GetID<void>())
        - use GetID<YourClass>() for your own private class-internal ID
        - use GetID<SomeStruct>() for your own private cross-class shareable ID

        ((  This is implementation is more general and uses one less temporary copy
            than the implementation found in several places of the SDK.
            NOTE: remove this paragraph once the SDK has been updated  ))

    Example:

        struct foo; // used for sharing an ID between A and B

        class A
        {
        public:
            unsigned int X(){return GetID(); };         // application-global
            unsigned int Y(){return GetID<A>(); };      // private for A
            unsigned int Z(){return GetID<foo>(); };    // shared with B

        };

        class B
        {
        public:
            unsigned int Y(){return GetID<B>(); };      // private for B
            unsigned int Z(){return GetID<foo>(); };    // shared with A
        };

        In this example, A::X() will return a counter that is globally unique throughout the lifetime of the application.
        A::Y() and B::Y() will return counters that increment independently for A and B. In other words,
        B does not know about A's counter, nor can it influence it.
        A::Z() and B::Z() will return a shared counter that increments if either A or B is asked to return a value.
*/

class ID
{
    wxIntPtr value;

    ID(wxIntPtr in) : value(in) {};

    template<typename> friend ID GetID();
    friend ID ConstructID(wxIntPtr);

public:

    ID() : value ((wxIntPtr) -1) {};

    operator wxIntPtr() const { return value; };
    operator void*() const { return reinterpret_cast<void*>(static_cast<uintptr_t>(value)); };

    bool Valid() const { return value != ((wxIntPtr) -1); };
    bool operator!() const { return !Valid(); };

    friend bool operator==(ID a, ID b)    { return a.value      == b.value; };
    friend bool operator==(ID a, int b)   { return a.value      == (wxIntPtr) b; };

    friend bool operator!=(ID a, ID b)    { return a.value      != b.value; };
    friend bool operator!=(ID a, int b)   { return a.value      != (wxIntPtr) b; };
};


template<typename whatever> inline ID GetID()
{
    static wxIntPtr id = (wxIntPtr) -1;
    return ID(++id);
}

inline ID GetID() { return GetID<void>(); }
inline ID ConstructID(wxIntPtr i) { return ID(i); }

// Just included to possibly set _LIBCPP_VERSION
// The header <ciso646> is deprecated in C++17 and removed in C++20
// and header <version> should be used instead. // patch 1511 2025/03/23
#if defined(__cplusplus) && __cplusplus < 201703L
#include <ciso646>
#else
#include <version>
#endif

#include <memory>

// Add std::shared_ptr in a namespace, so different implementations can be used with different compilers
namespace cb
{
    using std::shared_ptr;
    using std::static_pointer_cast;
    using std::weak_ptr;
}

#if defined(__APPLE__) && defined(__MACH__)
    #define CB_LIBRARY_ENVVAR _T("DYLD_LIBRARY_PATH")
#elif !defined(__WXMSW__)
    #define CB_LIBRARY_ENVVAR _T("LD_LIBRARY_PATH")
#else
    #define CB_LIBRARY_ENVVAR _T("PATH")
#endif

#endif
