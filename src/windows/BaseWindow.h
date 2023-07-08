#pragma once

#include <gtkmm.h>

namespace DV
{

//! \brief Base class for all window classes
//!
//! Contains common shutdown related stuff
class BaseWindow
{
public:
    virtual ~BaseWindow() = default;

    void Close();

    //! Default callback for gtk close events
    //! Use when closing doesn't need to be prevented
    //! \protected
    virtual bool _OnClosed(GdkEventAny* event);

protected:
    //! \brief Reports to DualView that this window has been closed and should be deleted
    virtual void _ReportClosed();

    //! \brief Do what Close does
    virtual void _OnClose() = 0;

protected:
    //! Makes sure _ReportClosed only sends one message
    //! \todo Make this atomic
    bool HasSentCloseReport = false;
};

//! \brief Object passed to the main thread when a window has closed
struct WindowClosedEvent
{
    explicit WindowClosedEvent(BaseWindow* window) : AffectedWindow(window)
    {
    }

    //! The window that sent this message
    BaseWindow* AffectedWindow;
};

} // namespace DV
