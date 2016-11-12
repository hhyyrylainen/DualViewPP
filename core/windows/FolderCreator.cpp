// ------------------------------------ //
#include "FolderCreator.h"

#include "Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //
//! \brief Constructor called by glade builder when loading a widget of this type
FolderCreator::FolderCreator(const VirtualPath &path, const std::string &prefillnewname){

    PathEntry.set_text(path.GetPathString());
    get_vbox()->add(PathEntry);
    
    NameContainter.set_orientation(Gtk::ORIENTATION_HORIZONTAL);

    NameLabel.set_text("New Folder:");
    NameContainter.add(NameLabel);

    NameEntry.set_text(prefillnewname);
    NameEntry.set_activates_default(true);
    NameContainter.add(NameEntry);
    NameContainter.child_property_expand(NameEntry) = true;

    get_vbox()->add(NameContainter);

    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    auto* create = add_button("_Create", Gtk::RESPONSE_OK);

    create->set_can_default(true);
    create->grab_default();

    show_all_children();

    set_focus(NameEntry);

    set_size_request(300, 150);
}

FolderCreator::~FolderCreator(){

}
// ------------------------------------ //
void FolderCreator::GetNewName(std::string &name, VirtualPath &parentpath){

    name = NameEntry.get_text();
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(name);

    parentpath = VirtualPath(PathEntry.get_text());
}
