#pragma once

#include "ListItem.h"

#include <memory>

namespace DV {

class Folder;

//! \brief Widget type for FolderPreview
class FolderListItem : public ListItem {
public:
    explicit FolderListItem(const std::shared_ptr<ItemSelectable>& selectable,
        std::shared_ptr<Folder> shownfolder = nullptr);

    //! \brief Sets the shown folder
    void SetFolder(std::shared_ptr<Folder> folder);

    const auto& GetFolder() const noexcept
    {
        return CurrentFolder;
    }

    //! Used to handle opening folders
    void _DoPopup() override;

    //! Changes layout depending on size
    void SetItemSize(LIST_ITEM_SIZE newsize) override;

protected:
    bool _OnRightClick(GdkEventButton* causedbyevent) override;

    void _OpenRemoveFromFolders();
    void _OpenAddToFolder();
    void _OpenRename();

    void _StartDelete();
    void _OnDeleteInfoReady(int totalContained, int wouldBeAddedToRoot);
    void _OnDeleteFailed(const std::string& message);

    void _DeleteTheFolder();

private:
    std::shared_ptr<Folder> CurrentFolder;

    //! Context menu for right click
    Gtk::Menu ContextMenu;
    Gtk::MenuItem ItemAddToFolder;
    Gtk::MenuItem ItemRemoveFromFolders;
    Gtk::SeparatorMenuItem ItemSeparator;
    Gtk::MenuItem ItemRename;
    Gtk::SeparatorMenuItem ItemSeparator2;
    Gtk::MenuItem ItemDelete;
};

} // namespace DV
