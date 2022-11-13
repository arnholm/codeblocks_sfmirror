SUBDIRS = src/resources

.rc.lo:
			$(LIBTOOL) --tag=RC --mode=compile $(WX_RESCOMP) $< -o $@

AM_CPPFLAGS = \
			$(WX_CXXFLAGS) \
			-I$(top_srcdir)/src/include \
			-I$(top_srcdir)/src/sdk/wxscintilla/include \
			-I$(top_srcdir)/src/include/tinyxml \
			-Isrc \
			-Isrc/LSPclient \
			-Isrc/codecompletion \
			-Isrc/unixprocess

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = \
			libclangd_client.la

libclangd_client_la_LDFLAGS = \
			@MODULE_SHARED_LDFLAGS@ \
			-no-undefined \
			-avoid-version

libclangd_client_la_LIBADD = \
			$(top_srcdir)/src/sdk/libcodeblocks.la \
			-lutil \
			$(WX_LIBS)

libclangd_client_la_SOURCES = \
			src/ClangLocator.cpp \
			src/codecompletion/ccoptionsdlg.cpp \
			src/codecompletion/ccoptionsprjdlg.cpp \
			src/codecompletion/cctreectrl.cpp \
			src/codecompletion/classbrowser.cpp \
			src/codecompletion/classbrowserbuilderthread.cpp \
			src/codecompletion/codecompletion.cpp \
			src/codecompletion/coderefactoring.cpp \
			src/codecompletion/doxygen_parser.cpp \
			src/codecompletion/gotofunctiondlg.cpp \
			src/codecompletion/insertclassmethoddlg.cpp \
			src/codecompletion/parsemanager.cpp \
			src/codecompletion/parsemanager_base.cpp \
			src/codecompletion/parser/ccdebuginfo.cpp \
			src/codecompletion/parser/cclogger.cpp \
			src/codecompletion/parser/expression.cpp \
			src/codecompletion/parser/LSP_symbolsparser.cpp \
			src/codecompletion/parser/LSP_tokenizer.cpp \
			src/codecompletion/parser/parser.cpp \
			src/codecompletion/parser/parser_base.cpp \
			src/codecompletion/parser/profiletimer.cpp \
			src/codecompletion/parser/searchtree.cpp \
			src/codecompletion/parser/token.cpp \
			src/codecompletion/parser/tokenizer.cpp \
			src/codecompletion/parser/tokentree.cpp \
			src/codecompletion/selectincludefile.cpp \
			src/codecompletion/wxsmith/CCTestFrame.wxs \
			src/LSPclient/client.cpp \
			src/LSPclient/lspdiagresultslog.cpp \
			src/unixprocess/asyncprocess/UnixProcess.cpp \
			src/unixprocess/asyncThreadTypes.cpp \
			src/unixprocess/fileutils.cpp \
			src/unixprocess/procutils.cpp \
			src/unixprocess/StringUtils.cpp
noinst_HEADERS = \
			src/ClangLocator.h \
			src/ClgdCCToken.h \
			src/codecompletion/ccoptionsdlg.h \
			src/codecompletion/ccoptionsprjdlg.h \
			src/codecompletion/cctreectrl.h \
			src/codecompletion/classbrowser.h \
			src/codecompletion/classbrowserbuilderthread.h \
			src/codecompletion/codecompletion.h \
			src/codecompletion/coderefactoring.h \
			src/codecompletion/doxygen_parser.h \
			src/codecompletion/gotofunctiondlg.h \
			src/codecompletion/IdleCallbackHandler.h \
			src/codecompletion/insertclassmethoddlg.h \
			src/codecompletion/LSPEventCallbackHandler.h \
			src/codecompletion/parsemanager.h \
			src/codecompletion/parsemanager_base.h \
			src/codecompletion/parser/ccdebuginfo.h \
			src/codecompletion/parser/cclogger.h \
			src/codecompletion/parser/expression.h \
			src/codecompletion/parser/LSP_symbolsparser.h \
			src/codecompletion/parser/LSP_tokenizer.h \
			src/codecompletion/parser/parser.h \
			src/codecompletion/parser/parser_base.h \
			src/codecompletion/parser/profiletimer.h \
			src/codecompletion/parser/searchtree.h \
			src/codecompletion/parser/token.h \
			src/codecompletion/parser/tokenizer.h \
			src/codecompletion/parser/tokentree.h \
			src/codecompletion/selectincludefile.h \
			src/LSPclient/client.h \
			src/LSPclient/json.hpp \
			src/LSPclient/protocol.h \
			src/LSPclient/transport.h \
			src/LSPclient/uri.h \
			src/LSPclient/lspdiagresultslog.h \
			src/LSP_SymbolKind.h \
			src/Version.h \
			src/unixprocess/asyncprocess/UnixProcess.h \
			src/unixprocess/asyncThreadTypes.h \
			src/unixprocess/fileutils.h \
			src/unixprocess/procutils.h \
			src/unixprocess/StringUtils.h \
			src/unixprocess/wxStringHash.h
