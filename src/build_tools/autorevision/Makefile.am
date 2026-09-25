# Build the autorevision tool using the build-system compiler.
auto_revision$(EXEEXT): autorevision.cpp
	$(CXX_FOR_BUILD) $(CXXCPPFLAGS_FOR_BUILD) $(CXXFLAGS_FOR_BUILD) $(LDFLAGS_FOR_BUILD) -o $@ $<

CLEANFILES = auto_revision$(EXEEXT)

all-local: auto_revision$(EXEEXT)
	./auto_revision$(EXEEXT) +int +wx +t $(top_srcdir) $(top_builddir)/src/include/autorevision.h
