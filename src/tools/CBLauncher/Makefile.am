EXTRA_DIST = 	$(srcdir)/doc/* \
		$(srcdir)/icons/* \
		$(srcdir)/*.cbp \
		icon.rc \
		mainpage.h \
		launcher.cpp


# CODEBLOCKS_NT is defined when we build C::B under the msys2/shell/configure/make
if CODEBLOCKS_NT

bin_PROGRAMS = CbLauncher

# Add common CPPFLAGS
AM_CPPFLAGS = -D_UNICODE -DUNICODE -DWINVER=0x0500 -Wall -Wextra -pedantic -Os -fexceptions

CbLauncher_LDFLAGS = -s -mwindows

CbLauncher_SOURCES = launcher.cpp

CbLauncher_LDADD = -lgdi32 -loleaut32

CbLauncher_LDADD += resources.$(OBJEXT)

# Resource file
CbLauncher_RESOURCES = icon.rc

# Optionally, add Windres if icon.rc needs to be compiled into the executable
CbLauncher_RESOURCES_FLAGS = --use-temp-file --input-format=rc --output-format=coff

# Rule to handle .rc files (if using Windres)
resources.$(OBJEXT): $(CbLauncher_RESOURCES)
	$(WINDRES) $(CbLauncher_RESOURCES_FLAGS) -o $@ $<
endif
