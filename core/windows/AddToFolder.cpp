// ------------------------------------ //
#include "AddToFolder.h"

#include "core/resources/Collection.h"

#include "core/DualView.h"

using namespace DV;
// ------------------------------------ //


AddToFolder::AddToFolder(std::shared_ptr<Collection> collection) :
    MovedCollection(collection),
    
    MainBox(Gtk::ORIENTATION_VERTICAL),
    ButtonBox(Gtk::ORIENTATION_HORIZONTAL),
    Accept(Gtk::StockID("gtk-ok")),
    Cancel(Gtk::StockID("gtk-cancel"))
{
    add(MainBox);
    MainBox.pack_start(TargetFolder, true, true);

    MainBox.pack_end(ButtonBox, false, true);

    ButtonBox.add(Cancel);
    ButtonBox.add(Accept);

    Accept.set_margin_left(2);
    Accept.set_always_show_image();
    Accept.set_size_request(120, 25);
    Cancel.set_always_show_image();

    Cancel.signal_clicked().connect(sigc::mem_fun(this, &AddToFolder::close));
    Accept.signal_clicked().connect(sigc::mem_fun(this, &AddToFolder::_OnApply));

    Accept.set_can_default();
    Accept.grab_default();

    ButtonBox.set_halign(Gtk::ALIGN_END);

    show_all_children();

    set_default_size(850, 450);
}

AddToFolder::~AddToFolder(){

    Close();
}
// ------------------------------------ //
void AddToFolder::_OnClose(){

    
}
// ------------------------------------ //
void AddToFolder::_OnApply(){

    LOG_INFO("AddToFolder: collection " + MovedCollection->GetName() + " to folder: " +
        static_cast<std::string>(TargetFolder.GetPath()));

    DualView::Get().AddCollectionToFolder(DualView::Get().GetFolderFromPath(
            TargetFolder.GetPath()), MovedCollection, true);
    
    close();
}
