AM_CPPFLAGS = $(WX_CXXFLAGS) \
				-I$(top_srcdir)/src/sdk/wxscintilla/include \
				-I$(top_srcdir)/src/include \
				-I$(top_srcdir)/src/include/tinyxml

if CODEBLOCKS_NT
AM_CPPFLAGS += -DEXPORT_LIB
endif

noinst_LTLIBRARIES = libtinyxml.la

libtinyxml_la_SOURCES = tinyxml.cpp tinyxmlerror.cpp tinyxmlparser.cpp

EXTRA_DIST = tinystr.cpp
