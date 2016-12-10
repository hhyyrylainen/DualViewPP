// ------------------------------------ //
#include "RemoveFromFolders.h"

using namespace DV;
// ------------------------------------ //

RemoveFromFolders::RemoveFromFolders(std::shared_ptr<Collection> collection) :
    TargetCollection(collection),
    
    MainBox(Gtk::ORIENTATION_VERTICAL),
    ApplyButton(Gtk::StockID("gtk-apply"))
{
    ApplyButton.set_always_show_image();
    MainBox.pack_end(ApplyButton, false, true);
    add(MainBox);

    ApplyButton.signal_clicked().connect(sigc::mem_fun(this, &RemoveFromFolders::close));
    
    show_all_children();

    set_default_size(600, 750);
}

RemoveFromFolders::~RemoveFromFolders(){

    Close();
}
// ------------------------------------ //
void RemoveFromFolders::_OnClose(){

    
}
