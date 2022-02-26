#include "DisplayEvents.h"
#include <manager.h>
#include <logmanager.h>
#include <configurationpanel.h>
#include <cbfunctor.h>
#include <globals.h>
#include <wx/datetime.h>
#include <wx/string.h>

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<DisplayEvents> reg("DisplayEvents");
}

// constructor
DisplayEvents::DisplayEvents()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if (!Manager::Get()->LoadResource("DisplayEvents.zip"))
        NotifyMissingFile("DisplayEvents.zip");
}

// destructor
DisplayEvents::~DisplayEvents()
{
}

void DisplayEvents::OnAttach()
{
    // do whatever initialization you need for your plugin
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be TRUE...
    // You should check for it in other functions, because if it
    // is FALSE, it means that the application did *not* "load"
    // (see: does not need) this plugin...
    Manager* pm = Manager::Get();
    pm->RegisterEventSink(cbEVT_APP_STARTUP_DONE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_APP_START_SHUTDOWN, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_APP_ACTIVATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_APP_DEACTIVATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_APP_CMDLINE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PLUGIN_ATTACHED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PLUGIN_RELEASED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PLUGIN_INSTALLED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PLUGIN_UNINSTALLED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PLUGIN_LOADING_COMPLETE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_CLOSE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_OPEN, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_SWITCHED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_ACTIVATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_DEACTIVATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_BEFORE_SAVE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_SAVE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_MODIFIED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_TOOLTIP, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_TOOLTIP_CANCEL, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_SPLIT, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_UNSPLIT, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_UPDATE_UI, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_EDITOR_CC_DONE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_NEW, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_CLOSE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_OPEN, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_SAVE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_ACTIVATE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_BEGIN_ADD_FILES, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_END_ADD_FILES, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_BEGIN_REMOVE_FILES, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_END_REMOVE_FILES, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_ADDED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_REMOVED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_RENAMED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_POPUP_MENU, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_TARGETS_MODIFIED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_RENAMED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PROJECT_OPTIONS_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_WORKSPACE_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_WORKSPACE_LOADING_COMPLETE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_WORKSPACE_CLOSING_BEGIN, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_WORKSPACE_CLOSING_COMPLETE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_BUILDTARGET_ADDED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_BUILDTARGET_REMOVED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_BUILDTARGET_RENAMED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_BUILDTARGET_SELECTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PIPEDPROCESS_STDOUT, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PIPEDPROCESS_STDERR, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_PIPEDPROCESS_TERMINATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_THREADTASK_STARTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_THREADTASK_ENDED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_THREADTASK_ALLDONE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_MENUBAR_CREATE_BEGIN, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_MENUBAR_CREATE_END, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPILER_STARTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPILER_FINISHED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPILER_SET_BUILD_OPTIONS, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_CLEAN_PROJECT_STARTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_CLEAN_WORKSPACE_STARTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPILER_SETTINGS_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPILE_FILE_REQUEST, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_STARTED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_PAUSED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_CONTINUED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_FINISHED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_CURSOR_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_DEBUGGER_UPDATED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_COMPLETE_CODE, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_SHOW_CALL_TIP, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));
    pm->RegisterEventSink(cbEVT_SETTINGS_CHANGED, new cbEventFunctor<DisplayEvents, CodeBlocksEvent>(this, &DisplayEvents::OnEventOccured));

    pm->RegisterEventSink(cbEVT_ADD_DOCK_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksDockEvent>(this, &DisplayEvents::OnDockEventOccured));
    pm->RegisterEventSink(cbEVT_REMOVE_DOCK_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksDockEvent>(this, &DisplayEvents::OnDockEventOccured));
    pm->RegisterEventSink(cbEVT_SHOW_DOCK_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksDockEvent>(this, &DisplayEvents::OnDockEventOccured));
    pm->RegisterEventSink(cbEVT_HIDE_DOCK_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksDockEvent>(this, &DisplayEvents::OnDockEventOccured));
    pm->RegisterEventSink(cbEVT_DOCK_WINDOW_VISIBILITY, new cbEventFunctor<DisplayEvents, CodeBlocksDockEvent>(this, &DisplayEvents::OnDockEventOccured));

    pm->RegisterEventSink(cbEVT_UPDATE_VIEW_LAYOUT, new cbEventFunctor<DisplayEvents, CodeBlocksLayoutEvent>(this, &DisplayEvents::OnLayoutEventOccured));
    pm->RegisterEventSink(cbEVT_QUERY_VIEW_LAYOUT, new cbEventFunctor<DisplayEvents, CodeBlocksLayoutEvent>(this, &DisplayEvents::OnLayoutEventOccured));
    pm->RegisterEventSink(cbEVT_SWITCH_VIEW_LAYOUT, new cbEventFunctor<DisplayEvents, CodeBlocksLayoutEvent>(this, &DisplayEvents::OnLayoutEventOccured));
    pm->RegisterEventSink(cbEVT_SWITCHED_VIEW_LAYOUT, new cbEventFunctor<DisplayEvents, CodeBlocksLayoutEvent>(this, &DisplayEvents::OnLayoutEventOccured));

    pm->RegisterEventSink(cbEVT_ADD_LOG_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_REMOVE_LOG_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_HIDE_LOG_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_SWITCH_TO_LOG_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_GET_ACTIVE_LOG_WINDOW, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_SHOW_LOG_MANAGER, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_HIDE_LOG_MANAGER, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_LOCK_LOG_MANAGER, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
    pm->RegisterEventSink(cbEVT_UNLOCK_LOG_MANAGER, new cbEventFunctor<DisplayEvents, CodeBlocksLogEvent>(this, &DisplayEvents::OnLogEventOccured));
}

void DisplayEvents::OnEventOccured(CodeBlocksEvent& event)
{
    wxDateTime now=wxDateTime::UNow();
    wxString msg = now.FormatISOTime();
    msg.Append(wxString::Format(",%03d  =>  ", now.GetMillisecond()));

    wxEventType type = event.GetEventType();
    if (type == cbEVT_APP_STARTUP_DONE) msg.Append("cbEVT_APP_STARTUP_DONE");
    else if (type == cbEVT_APP_START_SHUTDOWN) msg.Append("cbEVT_APP_START_SHUTDOWN");
    else if (type == cbEVT_APP_ACTIVATED) msg.Append("cbEVT_APP_ACTIVATED");
    else if (type == cbEVT_APP_DEACTIVATED) msg.Append("cbEVT_APP_DEACTIVATED");
    else if (type == cbEVT_APP_CMDLINE) msg.Append("cbEVT_APP_CMDLINE");
    else if (type == cbEVT_PLUGIN_ATTACHED) msg.Append("cbEVT_PLUGIN_ATTACHED");
    else if (type == cbEVT_PLUGIN_RELEASED) msg.Append("cbEVT_PLUGIN_RELEASED");
    else if (type == cbEVT_PLUGIN_INSTALLED) msg.Append("cbEVT_PLUGIN_INSTALLED");
    else if (type == cbEVT_PLUGIN_UNINSTALLED) msg.Append("cbEVT_PLUGIN_UNINSTALLED");
    else if (type == cbEVT_PLUGIN_LOADING_COMPLETE) msg.Append("cbEVT_PLUGIN_LOADING_COMPLETE");
    else if (type == cbEVT_EDITOR_CLOSE) msg.Append("cbEVT_EDITOR_CLOSE");
    else if (type == cbEVT_EDITOR_OPEN) msg.Append("cbEVT_EDITOR_OPEN");
    else if (type == cbEVT_EDITOR_SWITCHED) msg.Append("cbEVT_EDITOR_SWITCHED");
    else if (type == cbEVT_EDITOR_ACTIVATED) msg.Append("cbEVT_EDITOR_ACTIVATED");
    else if (type == cbEVT_EDITOR_DEACTIVATED) msg.Append("cbEVT_EDITOR_DEACTIVATED");
    else if (type == cbEVT_EDITOR_BEFORE_SAVE) msg.Append("cbEVT_EDITOR_BEFORE_SAVE");
    else if (type == cbEVT_EDITOR_SAVE) msg.Append("cbEVT_EDITOR_SAVE");
    else if (type == cbEVT_EDITOR_MODIFIED) msg.Append("cbEVT_EDITOR_MODIFIED");
    else if (type == cbEVT_EDITOR_TOOLTIP) msg.Append("cbEVT_EDITOR_TOOLTIP");
    else if (type == cbEVT_EDITOR_TOOLTIP_CANCEL) msg.Append("cbEVT_EDITOR_TOOLTIP_CANCEL");
    else if (type == cbEVT_EDITOR_SPLIT) msg.Append("cbEVT_EDITOR_SPLIT");
    else if (type == cbEVT_EDITOR_UNSPLIT) msg.Append("cbEVT_EDITOR_UNSPLIT");
    else if (type == cbEVT_EDITOR_UPDATE_UI) msg.Append("cbEVT_EDITOR_UPDATE_UI");
    else if (type == cbEVT_EDITOR_CC_DONE) msg.Append("cbEVT_EDITOR_CC_DONE");
    else if (type == cbEVT_PROJECT_NEW) msg.Append("cbEVT_PROJECT_NEW");
    else if (type == cbEVT_PROJECT_CLOSE) msg.Append("cbEVT_PROJECT_CLOSE");
    else if (type == cbEVT_PROJECT_OPEN) msg.Append("cbEVT_PROJECT_OPEN");
    else if (type == cbEVT_PROJECT_SAVE) msg.Append("cbEVT_PROJECT_SAVE");
    else if (type == cbEVT_PROJECT_ACTIVATE) msg.Append("cbEVT_PROJECT_ACTIVATE");
    else if (type == cbEVT_PROJECT_BEGIN_ADD_FILES) msg.Append("cbEVT_PROJECT_BEGIN_ADD_FILES");
    else if (type == cbEVT_PROJECT_END_ADD_FILES) msg.Append("cbEVT_PROJECT_END_ADD_FILES");
    else if (type == cbEVT_PROJECT_BEGIN_REMOVE_FILES) msg.Append("cbEVT_PROJECT_BEGIN_REMOVE_FILES");
    else if (type == cbEVT_PROJECT_END_REMOVE_FILES) msg.Append("cbEVT_PROJECT_END_REMOVE_FILES");
    else if (type == cbEVT_PROJECT_FILE_ADDED) msg.Append("cbEVT_PROJECT_FILE_ADDED");
    else if (type == cbEVT_PROJECT_FILE_REMOVED) msg.Append("cbEVT_PROJECT_FILE_REMOVED");
    else if (type == cbEVT_PROJECT_FILE_RENAMED) msg.Append("cbEVT_PROJECT_FILE_RENAMED");
    else if (type == cbEVT_PROJECT_POPUP_MENU) msg.Append("cbEVT_PROJECT_POPUP_MENU");
    else if (type == cbEVT_PROJECT_TARGETS_MODIFIED) msg.Append("cbEVT_PROJECT_TARGETS_MODIFIED");
    else if (type == cbEVT_PROJECT_RENAMED) msg.Append("cbEVT_PROJECT_RENAMED");
    else if (type == cbEVT_PROJECT_OPTIONS_CHANGED) msg.Append("cbEVT_PROJECT_OPTIONS_CHANGED");
    else if (type == cbEVT_WORKSPACE_CHANGED) msg.Append("cbEVT_WORKSPACE_CHANGED");
    else if (type == cbEVT_WORKSPACE_LOADING_COMPLETE) msg.Append("cbEVT_WORKSPACE_LOADING_COMPLETE");
    else if (type == cbEVT_WORKSPACE_CLOSING_BEGIN) msg.Append("cbEVT_WORKSPACE_CLOSING_BEGIN");
    else if (type == cbEVT_WORKSPACE_CLOSING_COMPLETE) msg.Append("cbEVT_WORKSPACE_CLOSING_COMPLETE");
    else if (type == cbEVT_BUILDTARGET_ADDED) msg.Append("cbEVT_BUILDTARGET_ADDED");
    else if (type == cbEVT_BUILDTARGET_REMOVED) msg.Append("cbEVT_BUILDTARGET_REMOVED");
    else if (type == cbEVT_BUILDTARGET_RENAMED) msg.Append("cbEVT_BUILDTARGET_RENAMED");
    else if (type == cbEVT_BUILDTARGET_SELECTED) msg.Append("cbEVT_BUILDTARGET_SELECTED");
    else if (type == cbEVT_PIPEDPROCESS_STDOUT) msg.Append("cbEVT_PIPEDPROCESS_STDOUT");
    else if (type == cbEVT_PIPEDPROCESS_STDERR) msg.Append("cbEVT_PIPEDPROCESS_STDERR");
    else if (type == cbEVT_PIPEDPROCESS_TERMINATED) msg.Append("cbEVT_PIPEDPROCESS_TERMINATED");
    else if (type == cbEVT_THREADTASK_STARTED) msg.Append("cbEVT_THREADTASK_STARTED");
    else if (type == cbEVT_THREADTASK_ENDED) msg.Append("cbEVT_THREADTASK_ENDED");
    else if (type == cbEVT_THREADTASK_ALLDONE) msg.Append("cbEVT_THREADTASK_ALLDONE");
    else if (type == cbEVT_MENUBAR_CREATE_BEGIN) msg.Append("cbEVT_MENUBAR_CREATE_BEGIN");
    else if (type == cbEVT_MENUBAR_CREATE_END) msg.Append("cbEVT_MENUBAR_CREATE_END");
    else if (type == cbEVT_COMPILER_STARTED) msg.Append("cbEVT_COMPILER_STARTED");
    else if (type == cbEVT_COMPILER_FINISHED) msg.Append("cbEVT_COMPILER_FINISHED");
    else if (type == cbEVT_COMPILER_SET_BUILD_OPTIONS) msg.Append("cbEVT_COMPILER_SET_BUILD_OPTIONS");
    else if (type == cbEVT_CLEAN_PROJECT_STARTED) msg.Append("cbEVT_CLEAN_PROJECT_STARTED");
    else if (type == cbEVT_CLEAN_WORKSPACE_STARTED) msg.Append("cbEVT_CLEAN_WORKSPACE_STARTED");
    else if (type == cbEVT_COMPILER_SETTINGS_CHANGED) msg.Append("cbEVT_COMPILER_SETTINGS_CHANGED");
    else if (type == cbEVT_COMPILE_FILE_REQUEST) msg.Append("cbEVT_COMPILE_FILE_REQUEST");
    else if (type == cbEVT_DEBUGGER_STARTED) msg.Append("cbEVT_DEBUGGER_STARTED");
    else if (type == cbEVT_DEBUGGER_PAUSED) msg.Append("cbEVT_DEBUGGER_PAUSED");
    else if (type == cbEVT_DEBUGGER_CONTINUED) msg.Append("cbEVT_DEBUGGER_CONTINUED");
    else if (type == cbEVT_DEBUGGER_FINISHED) msg.Append("cbEVT_DEBUGGER_FINISHED");
    else if (type == cbEVT_DEBUGGER_CURSOR_CHANGED) msg.Append("cbEVT_DEBUGGER_CURSOR_CHANGED");
    else if (type == cbEVT_DEBUGGER_UPDATED) msg.Append("cbEVT_DEBUGGER_UPDATED");
    else if (type == cbEVT_COMPLETE_CODE) msg.Append("cbEVT_COMPLETE_CODE");
    else if (type == cbEVT_SHOW_CALL_TIP) msg.Append("cbEVT_SHOW_CALL_TIP");
    else if (type == cbEVT_SETTINGS_CHANGED) msg.Append("cbEVT_SETTINGS_CHANGED");
    else msg.Append(_("unknown CodeBlocksEvent"));
    Manager::Get()->GetLogManager()->DebugLog(msg);
}

void DisplayEvents::OnDockEventOccured(CodeBlocksDockEvent& event)
{
    wxDateTime now=wxDateTime::UNow();
    wxString msg = now.FormatISOTime();
    msg.Append(wxString::Format(",%03d  =>  ", now.GetMillisecond()));

    wxEventType type=event.GetEventType();
    if (type == cbEVT_ADD_DOCK_WINDOW) msg.Append("cbEVT_ADD_DOCK_WINDOW");
    else if (type == cbEVT_REMOVE_DOCK_WINDOW) msg.Append("cbEVT_REMOVE_DOCK_WINDOW");
    else if (type == cbEVT_SHOW_DOCK_WINDOW) msg.Append("cbEVT_SHOW_DOCK_WINDOW");
    else if (type == cbEVT_HIDE_DOCK_WINDOW) msg.Append("cbEVT_HIDE_DOCK_WINDOW");
    else if (type == cbEVT_DOCK_WINDOW_VISIBILITY) msg.Append("cbEVT_DOCK_WINDOW_VISIBILITY");
    else msg.Append(_("unknown CodeBLocksDockEvent"));
    Manager::Get()->GetLogManager()->DebugLog(msg);
}

void DisplayEvents::OnLayoutEventOccured(CodeBlocksLayoutEvent& event)
{
    wxDateTime now=wxDateTime::UNow();
    wxString msg = now.FormatISOTime();
    msg.Append(wxString::Format(",%03.d  =>  ", now.GetMillisecond()));

    wxEventType type=event.GetEventType();
    if (type == cbEVT_UPDATE_VIEW_LAYOUT) msg.Append("cbEVT_UPDATE_VIEW_LAYOUT");
    else if (type == cbEVT_QUERY_VIEW_LAYOUT) msg.Append("cbEVT_QUERY_VIEW_LAYOUT");
    else if (type == cbEVT_SWITCH_VIEW_LAYOUT) msg.Append("cbEVT_SWITCH_VIEW_LAYOUT");
    else if (type == cbEVT_SWITCHED_VIEW_LAYOUT) msg.Append("cbEVT_SWITCHED_VIEW_LAYOUT");
    else msg.Append(_("unknown CodeBlocksLayoutEvent"));
    Manager::Get()->GetLogManager()->DebugLog(msg);
}

void DisplayEvents::OnLogEventOccured(CodeBlocksLogEvent& event)
{
    wxDateTime now=wxDateTime::UNow();
    wxString msg = now.FormatISOTime();
    msg.Append(wxString::Format(",%03d  =>  ", now.GetMillisecond()));

    wxEventType type=event.GetEventType();
    if (type == cbEVT_ADD_LOG_WINDOW) msg.Append("cbEVT_ADD_LOG_WINDOW");
    else if (type == cbEVT_REMOVE_LOG_WINDOW) msg.Append("cbEVT_REMOVE_LOG_WINDOW");
    else if (type == cbEVT_HIDE_LOG_WINDOW) msg.Append("cbEVT_HIDE_LOG_WINDOW");
    else if (type == cbEVT_SWITCH_TO_LOG_WINDOW) msg.Append("cbEVT_SWITCH_TO_LOG_WINDOW");
    else if (type == cbEVT_GET_ACTIVE_LOG_WINDOW) msg.Append("cbEVT_GET_ACTIVE_LOG_WINDOW");
    else if (type == cbEVT_SHOW_LOG_MANAGER) msg.Append("cbEVT_SHOW_LOG_MANAGER");
    else if (type == cbEVT_HIDE_LOG_MANAGER) msg.Append("cbEVT_HIDE_LOG_MANAGER");
    else if (type == cbEVT_LOCK_LOG_MANAGER) msg.Append("cbEVT_LOCK_LOG_MANAGER");
    else if (type == cbEVT_UNLOCK_LOG_MANAGER) msg.Append("cbEVT_UNLOCK_LOG_MANAGER");
    else msg.Append(_("unknown CodeBlocksLogEvent"));
    Manager::Get()->GetLogManager()->DebugLog(msg);
}

void DisplayEvents::OnRelease(bool appShutDown)
{
    // do de-initialization for your plugin
    // if appShutDown is true, the plugin is unloaded because Code::Blocks is being shut down,
    // which means you must not use any of the SDK Managers
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be FALSE...
    Manager::Get()->RemoveAllEventSinksFor(this);
}
