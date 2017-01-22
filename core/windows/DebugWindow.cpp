// ------------------------------------ //
#include "DebugWindow.h"

#include "DualView.h"
#include "Database.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
DebugWindow::DebugWindow(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{

    signal_delete_event().connect(sigc::mem_fun(*this, &DebugWindow::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &DebugWindow::_OnHidden));

    Gtk::Button* MakeBusy;

    BUILDER_GET_WIDGET(MakeBusy);

    MakeBusy->signal_clicked().connect(sigc::mem_fun(*this, &DebugWindow::OnMakeDBBusy));
}

DebugWindow::~DebugWindow(){

}
// ------------------------------------ //
bool DebugWindow::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();
    return true;
}

void DebugWindow::_OnHidden(){
    
}
// ------------------------------------ //
void DebugWindow::OnMakeDBBusy(){

    DualView::Get().QueueDBThreadFunction([](){

            GUARD_LOCK_OTHER(DualView::Get().GetDatabase());
            std::this_thread::sleep_for(std::chrono::seconds(15));
        });
}
// ------------------------------------ //
