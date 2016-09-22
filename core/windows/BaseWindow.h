#pragma once


namespace DV{

//! \brief Base class for all window classes
//!
//! Contains common shutdown related stuff
class BaseWindow{
public:

    virtual ~BaseWindow() = default;

    virtual void Close() = 0;

protected:

    //! \brief Reports to DualView that this window has been closed and should be deleted
    virtual void _ReportClosed();

protected:

    //! Makes sure _ReportClosed only sends one message
    bool HasSentCloseReport = false;
};

//! \brief Object passed to the main thread when a window has closed
struct WindowClosedEvent{

    WindowClosedEvent(BaseWindow* window) : AffectedWindow(window){}
    
    //! The window that sent this messageb
    BaseWindow* AffectedWindow;
};

}
