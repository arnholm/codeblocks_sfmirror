AM_CPPFLAGS = $(WX_CXXFLAGS) \
		-I$(srcdir)/include

if CODEBLOCKS_NT
AM_CPPFLAGS += -DWXMAKINGDLL_THINGS
endif

wxcontribitemsdir=$(libdir)/codeblocks/wxContribItems

wxcontribitems_LTLIBRARIES = libwxcustombutton.la

libwxcustombutton_la_LDFLAGS = -no-undefined -version-info 0:1:0

libwxcustombutton_la_LIBADD = $(WX_LIBS)

libwxcustombutton_la_SOURCES = \
./src/toggle.cpp

sdkdir=$(includedir)/codeblocks/wxContribItems/wxthings/include/wx/things

sdk_HEADERS = \
./include/wx/things/toggle.h \
./include/wx/things/thingdef.h

EXTRA_DIST = ./src/precomp.cpp ./src/precomp.h
