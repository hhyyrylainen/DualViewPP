// ------------------------------------ //
#include "FolderListItem.h"

#include "core/resources/Folder.h"

using namespace DV;
// ------------------------------------ //
FolderListItem::FolderListItem(const ItemSelectable &selectable,
    std::shared_ptr<Folder> shownfolder /*= nullptr*/) :
    ListItem(nullptr, shownfolder ? shownfolder->GetName() : "", selectable, true),
    CurrentFolder(shownfolder)
{
    
}
// ------------------------------------ //
void FolderListItem::SetFolder(std::shared_ptr<Folder> folder){

    CurrentFolder = folder;
    _SetName(folder->GetName());
}
