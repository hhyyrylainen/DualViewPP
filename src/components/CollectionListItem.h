#pragma once

#include "ListItem.h"

#include <memory>

namespace DV {

class Collection;

//! \brief Widget type for CollectionPreview
//! \todo Switch preview icon loading to database thread if it risks hanging the main thread
//! (done already?)
class CollectionListItem : public ListItem {
public:
    CollectionListItem(const std::shared_ptr<ItemSelectable>& selectable,
        std::shared_ptr<Collection> showncollection = nullptr);

    //! \brief Sets the shown collection
    void SetCollection(std::shared_ptr<Collection> collection);

protected:
    void _DoPopup() override;

    bool _OnRightClick(GdkEventButton* causedbyevent) override;

    void _OpenRemoveFromFolders();
    void _OpenAddToFolder();
    void _OpenReorderView();

private:
    std::shared_ptr<Collection> CurrentCollection;

    //! Context menu for right click
    Gtk::Menu ContextMenu;
    Gtk::MenuItem ItemView;
    Gtk::SeparatorMenuItem ItemSeparator1;
    Gtk::MenuItem ItemAddToFolder;
    Gtk::MenuItem ItemRemoveFromFolders;
    Gtk::SeparatorMenuItem ItemSeparator2;
    Gtk::MenuItem ItemReorder;
};

} // namespace DV
