#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Folder;

//! \brief Widget type for FolderPreview
class FolderListItem : public ListItem{
public:

    
    FolderListItem(const ItemSelectable &selectable,
        std::shared_ptr<Folder> shownfolder = nullptr);

    //! \brief Sets the shown folder
    void SetFolder(std::shared_ptr<Folder> folder);


private:

    std::shared_ptr<Folder> CurrentFolder;
};

}
    
