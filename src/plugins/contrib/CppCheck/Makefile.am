SUBDIRS = resources

AM_CPPFLAGS = $(WX_CXXFLAGS) \
		-I$(top_srcdir)/src/include \
		-I$(top_srcdir)/src/sdk/wxscintilla/include

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = libCppCheck.la

libCppCheck_la_LDFLAGS = @MODULE_SHARED_LDFLAGS@ -version-info 0:1:0 -no-undefined -avoid-version

libCppCheck_la_LIBADD = ../../../sdk/libcodeblocks.la \
		$(WX_LIBS)

libCppCheck_la_SOURCES = CppCheck.cpp \
            CppCheckListLog.cpp \
            ConfigPanel.cpp

noinst_HEADERS = CppCheck.h \
            CppCheckListLog.h \
            ConfigPanel.h

EXTRA_DIST = $(srcdir)/*.cbp \
		wxsmith
