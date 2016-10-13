#pragma once


namespace DV{

//! \brief Base class for all window classes
//!
//! Contains common shutdown related stuff
class BaseWindow{
public:

    virtual ~BaseWindow() = default;

    void Close();

protected:

    //! \brief Reports to DualView that this window has been closed and should be deleted
    virtual void _ReportClosed();

    //! \brief Do what Close does
    virtual void _OnClose() = 0;

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
