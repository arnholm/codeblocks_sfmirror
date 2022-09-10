/***************************************************************
 * Name:      ThreadSearchViewManagerMessagesNotebook
 * Purpose:   Implements the ThreadSearchViewManagerBase
 *            interface to make the ThreadSearchView panel
 *            managed by the Messages notebook.
 * Author:    Jerome ANTOINE
 * Created:   2007-07-19
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/


#ifndef THREAD_SEARCH_VIEW_MANAGER_MESSAGES_NOTEBOOK_H
#define THREAD_SEARCH_VIEW_MANAGER_MESSAGES_NOTEBOOK_H

#include "logger.h"

#include "ThreadSearchViewManagerBase.h"

class wxBitmap;
#if wxCHECK_VERSION(3, 1, 6)
class wxBitmapBundle;
#endif
class wxWindow;
class ThreadSearchView;
class ThreadSearchLogger;

class ThreadSearchViewManagerMessagesNotebook : public ThreadSearchViewManagerBase
{
public:
    ThreadSearchViewManagerMessagesNotebook(ThreadSearchView* pThreadSearchView)
        : ThreadSearchViewManagerBase(pThreadSearchView), m_Bitmap(nullptr)
    {}
    ~ThreadSearchViewManagerMessagesNotebook() override;

    eManagerTypes GetManagerType() override { return TypeMessagesNotebook; }
    void AddViewToManager() override;
    void RemoveViewFromManager() override;
    bool ShowView(uint32_t flags) override;
    bool IsViewShown() override;
    void Raise() override;
private:
#if wxCHECK_VERSION(3, 1, 6)
    wxBitmapBundle *m_Bitmap;
#else
    wxBitmap *m_Bitmap;
#endif
};

#endif // THREAD_SEARCH_VIEW_MANAGER_MESSAGES_NOTEBOOK_H
