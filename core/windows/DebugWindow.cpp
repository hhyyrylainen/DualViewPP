// ------------------------------------ //
#include "DebugWindow.h"

#include "DualView.h"
#include "Database.h"

#include "Common.h"

#include "CacheManager.h"

#include "resources/InternetImage.h"

#include <boost/filesystem.hpp>

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

    Gtk::Button* TestImageRead;

    BUILDER_GET_WIDGET(TestImageRead);

    TestImageRead->signal_clicked().connect(sigc::mem_fun(*this,
            &DebugWindow::OnTestImageRead));


    Gtk::Button* TestInstanceCreation;
    
    BUILDER_GET_WIDGET(TestInstanceCreation);

    TestInstanceCreation->signal_clicked().connect(sigc::mem_fun(*this,
            &DebugWindow::OnTestInstanceCreation));
    
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
void DebugWindow::OnTestImageRead(){

    constexpr auto filePath = "/home/hhyyrylainen/690806.jpg";

    LEVIATHAN_ASSERT(boost::filesystem::exists(filePath),
        "OnTestImageRead preset image is missing");

    int width, height;
    std::string extension;
        
    LEVIATHAN_ASSERT(CacheManager::GetImageSize(filePath, width, height, extension),
        "OnTestImageRead image size get failed");


    LEVIATHAN_ASSERT(CacheManager::GetImageSize("/home/hhyyrylainen/803085.png", width,
            height, extension),
        "OnTestImageRead image size get failed");
    
}
// ------------------------------------ //
void DebugWindow::OnTestInstanceCreation(){

    // This should be released once it goes out of scope
    
    auto img1 = InternetImage::Create(ScanFoundImage("http://test.com/img.jpg", "dualview"),
        false);
    
}
