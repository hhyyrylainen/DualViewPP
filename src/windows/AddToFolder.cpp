// ------------------------------------ //
#include "AddToFolder.h"

#include "resources/Collection.h"
#include "resources/Folder.h"

#include "DualView.h"

using namespace DV;
// ------------------------------------ //

AddToFolder::AddToFolder() :
    MainBox(Gtk::ORIENTATION_VERTICAL), ButtonBox(Gtk::ORIENTATION_HORIZONTAL),
    Accept("_Accept", true), Cancel("_Cancel", true)
{
    add(MainBox);
    MainBox.pack_start(TargetFolder, true, true);

    MainBox.pack_end(ButtonBox, false, true);

    ButtonBox.add(Cancel);
    ButtonBox.add(Accept);

    Cancel.signal_clicked().connect(sigc::mem_fun(this, &AddToFolder::close));
    Accept.signal_clicked().connect(sigc::mem_fun(this, &AddToFolder::_OnApply));

    Accept.set_can_default();
    Accept.grab_default();

    ButtonBox.set_halign(Gtk::ALIGN_END);

    show_all_children();

    set_default_size(850, 450);
}

AddToFolder::AddToFolder(std::shared_ptr<Folder> folder) : AddToFolder()
{
    AddedFolder = folder;
    _UpdateLabelsForType();
}

AddToFolder::AddToFolder(std::shared_ptr<Collection> collection) : AddToFolder()
{
    MovedCollection = collection;
    _UpdateLabelsForType();
}

AddToFolder::~AddToFolder()
{
    Close();
}
// ------------------------------------ //
void AddToFolder::_OnClose() {}
// ------------------------------------ //
void AddToFolder::_OnApply()
{
    const auto target = DualView::Get().GetFolderFromPath(TargetFolder.GetPath());

    if(MovedCollection) {
        LOG_INFO("AddToFolder: collection " + MovedCollection->GetName() +
                 " to folder: " + static_cast<std::string>(TargetFolder.GetPath()));

        DualView::Get().AddCollectionToFolder(target, MovedCollection, true);
    } else if(AddedFolder) {

        LOG_INFO("AddToFolder: folder " + AddedFolder->GetName() +
                 " to folder: " + static_cast<std::string>(TargetFolder.GetPath()));

        if(!target->AddFolder(AddedFolder))
            LOG_ERROR("Failed to add the folder to a folder");

        DualView::Get().AddCollectionToFolder(target, MovedCollection, true);

    } else {
        LOG_ERROR("Unknown type to add to folder");
    }

    close();
}
// ------------------------------------ //
void AddToFolder::_UpdateLabelsForType() {}
