AM_CPPFLAGS = $(WX_CXXFLAGS) \
		-I$(top_srcdir)/src/include

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = libDisplayEvents.la

libDisplayEvents_la_LDFLAGS = -module -version-info 0:1:0 -shared -no-undefined -avoid-version

libDisplayEvents_la_LIBADD = $(PLUGIN_WX_LIBS) \
							../../../sdk/libcodeblocks.la

libDisplayEvents_la_SOURCES = DisplayEvents.cpp

noinst_HEADERS = DisplayEvents.h

EXTRA_DIST = 	$(srcdir)/*.cbp \
		manifest.xml

pkgdata_DATA = DisplayEvents.zip

CLEANFILES = $(pkgdata_DATA)

IncrementalSearch.zip: manifest.xml
	PWD=`pwd` cd $(srcdir) && zip -R $(PWD)/DisplayEvents.zip manifest.xml > /dev/null
