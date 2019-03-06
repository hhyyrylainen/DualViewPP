#pragma once

#include "BaseWindow.h"

#include "IsAlive.h"
#include "components/FolderSelector.h"

#include <gtkmm.h>

namespace DV {

class Collection;

class RemoveFromFolders : public BaseWindow, public Gtk::Window, public IsAlive {

    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns()
        {
            add(m_keep_folder);
            add(m_folder_path);
        }

        Gtk::TreeModelColumn<Glib::ustring> m_folder_path;
        Gtk::TreeModelColumn<bool> m_keep_folder;
    };

public:
    RemoveFromFolders(std::shared_ptr<Collection> collection);
    ~RemoveFromFolders();

    //! \brief Reads the folders collection is in
    void ReadFolders();

protected:
    void _OnClose() override;

    void _OnApply();

    //! When a checkbox is toggled this handles that
    void _OnToggled(const Glib::ustring& path);

protected:
    std::shared_ptr<Collection> TargetCollection;

    Gtk::Box MainBox;

    Gtk::Button ApplyButton;


    // The selection widget //
    Gtk::TreeView FoldersTreeView;
    Glib::RefPtr<Gtk::ListStore> FoldersModel;
    ModelColumns TreeViewColumns;
};


} // namespace DV
