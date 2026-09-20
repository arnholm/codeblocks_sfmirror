noinst_PROGRAMS = auto_revision
auto_revision_SOURCES = autorevision.cpp

CC = $(CC_FOR_BUILD)
CFLAGS = $(CFLAGS_FOR_BUILD)
CXX = $(CXX_FOR_BUILD)

all-local: auto_revision$(EXEEXT)
	./auto_revision +int +wx +t $(top_srcdir) $(top_builddir)/src/include/autorevision.h
