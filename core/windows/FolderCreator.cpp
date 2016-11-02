// ------------------------------------ //
#include "FolderCreator.h"

using namespace DV;
// ------------------------------------ //
//! \brief Constructor called by glade builder when loading a widget of this type
FolderCreator::FolderCreator(const std::string path, const std::string &prefillnewname)
{
    get_vbox()->add(Container);
    
    Container.add(PathEntry);

    PathEntry.set_text(path);

    show_all_children();
}

FolderCreator::~FolderCreator(){

}
