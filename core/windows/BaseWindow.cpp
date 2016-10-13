// ------------------------------------ //
#include "BaseWindow.h"

#include "DualView.h"

using namespace DV;
// ------------------------------------ //

void BaseWindow::Close(){

    _OnClose();
    _ReportClosed();
}

void BaseWindow::_ReportClosed(){

    if(HasSentCloseReport)
        return;

    HasSentCloseReport = true;

    DualView::Get().WindowClosed(std::make_shared<WindowClosedEvent>(this));
}

