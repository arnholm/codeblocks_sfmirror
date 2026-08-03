SUBDIRS = src/resources

.rc.lo:
			$(LIBTOOL) --tag=RC --mode=compile $(WX_RESCOMP) $< -o $@

AM_CPPFLAGS = \
			$(WX_CXXFLAGS) \
			-I$(top_srcdir)/src/include \
			-I$(top_srcdir)/src/sdk/wxscintilla/include \
			-I$(top_srcdir)/src/include/tinyxml \
			-I$(srcdir)/src

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = \
			libdebugger_gdbmi.la

libdebugger_gdbmi_la_LDFLAGS = \
			@MODULE_SHARED_LDFLAGS@ \
			-no-undefined \
			-avoid-version

libdebugger_gdbmi_la_LIBADD = \
			../../../sdk/libcodeblocks.la \
			$(CB_TINYXML_LIBS) \
			$(WX_LIBS)

if ! CODEBLOCKS_NT
libdebugger_gdbmi_la_LIBADD += -lutil
endif

libdebugger_gdbmi_la_SOURCES = \
			src/actions.cpp \
			src/cJSON.cpp \
			src/cmd_queue.cpp \
			src/cmd_result_parser.cpp \
			src/cmd_result_tokens.cpp \
			src/databreakpointdlg.cpp \
			src/dbgGDBmi.cpp \
			src/debuggeroptionsdlg.cpp \
			src/debuggeroptionsprjdlg.cpp \
			src/definitions.cpp \
			src/editbreakpointdlg.cpp \
			src/editwatchdlg.cpp \
			src/escape.cpp \
			src/events.cpp \
			src/frame.cpp \
            src/gdb_executor.cpp \
            src/helpers.cpp \
            src/json_node.cpp \
            src/parsewatchvalue.cpp \
            src/updated_variable.cpp \
            src/resources/manifest.xml


noinst_HEADERS = \
			src/actions.h \
			src/cJSON.h \
			src/cmd_queue.h \
			src/cmd_result_parser.h \
			src/cmd_result_tokens.h \
			src/databreakpointdlg.h \
			src/dbgGDBmi.h \
			src/debuggeroptionsdlg.h \
			src/debuggeroptionsprjdlg.h \
			src/definitions.h \
			src/editbreakpointdlg.h \
			src/editwatchdlg.h \
			src/escape.h \
			src/events.h \
			src/frame.h \
            src/gdb_executor.h \
            src/helpers.h \
            src/json_node.h \
			src/parsewatchvalue.h \
            src/remotedebugging.h \
            src/resources/manifest.xml \
            src/updated_variable.cpp \
            src/updated_variable.h
