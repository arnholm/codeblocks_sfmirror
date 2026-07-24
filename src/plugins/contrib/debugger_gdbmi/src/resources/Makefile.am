# Ensure this matches the exact filename your C++ code is looking for
pkgdata_DATA = debugger_gdbmi.zip
CLEANFILES = $(pkgdata_DATA)

# List the files explicitly
DEBUG_RESOURCES = debugger_gdbmi_options_dlg.xrc \
                  manifest.xml

# Ensure these files are included in 'make dist'
EXTRA_DIST = $(DEBUG_RESOURCES)

# Rule to build the zip file safely and support out-of-tree builds
debugger_gdbmi.zip: $(DEBUG_RESOURCES)
	BASEDIR=`pwd`; \
	cd $(srcdir) && zip -j $$BASEDIR/debugger_gdbmi.zip $(DEBUG_RESOURCES)
