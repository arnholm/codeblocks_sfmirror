AM_CPPFLAGS = $(WX_CXXFLAGS) \
		-I$(top_srcdir)/src/include \
		-I$(top_srcdir)/src/sdk/wxscintilla/include

if CODEBLOCKS_NT
AM_CPPFLAGS += -DEXPORT_LIB
endif

noinst_LTLIBRARIES = libwxsmith_properties.la

libwxsmith_properties_la_LDFLAGS = @MODULE_SHARED_LDFLAGS@ -version-info 0:1:0 -no-undefined -avoid-version

libwxsmith_properties_la_LIBADD = ../../../../sdk/libcodeblocks.la

libwxsmith_properties_la_SOURCES = \
./wxsarraystringeditordlg.cpp \
./wxsboolproperty.cpp \
./wxspropertystream.cpp \
./wxsflagsproperty.cpp \
./wxsfloatproperty.cpp \
./wxsemptyproperty.cpp \
./wxsstringproperty.cpp \
./wxslongproperty.cpp \
./wxsenumproperty.cpp \
./wxsarraystringproperty.cpp \
./wxsquickpropspanel.cpp \
./wxsproperty.cpp \
./wxspropertygridmanager.cpp \
./wxscustomeditorproperty.cpp \
./wxspropertycontainer.cpp \
./wxstwolongproperty.cpp \
./wxseditenumproperty.cpp 


sdkdir=$(includedir)/wxsmith/properties

sdk_HEADERS = \
./wxspropertycontainer.h \
./wxsproperties.h \
./wxsboolproperty.h \
./wxspropertystream.h \
./wxsflagsproperty.h \
./wxsfloatproperty.h \
./wxsemptyproperty.h \
./wxslongproperty.h \
./wxsarraystringeditordlg.h \
./wxsenumproperty.h \
./wxsarraystringproperty.h \
./wxsstringproperty.h \
./wxsquickpropspanel.h \
./wxsproperty.h \
./wxspropertygridmanager.h \
./wxscustomeditorproperty.h \
./wxstwolongproperty.h \
./wxseditenumproperty.h
