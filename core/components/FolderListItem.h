#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Folder;

//! \brief Widget type for FolderPreview
class FolderListItem : public ListItem{
public:

    
    FolderListItem(const std::shared_ptr<ItemSelectable> &selectable,
        std::shared_ptr<Folder> shownfolder = nullptr);

    //! \brief Sets the shown folder
    void SetFolder(std::shared_ptr<Folder> folder);

    auto GetFolder() const{

        return CurrentFolder;
    }

    //! Used to handle opening folders
    void _DoPopup() override;

    //! Changes layout depending on size
    void SetItemSize(LIST_ITEM_SIZE newsize) override;

private:

    std::shared_ptr<Folder> CurrentFolder;
};

}
    
