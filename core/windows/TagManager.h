#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows all the tags and allows editing them
class TagManager : public Gtk::Window, public IsAlive{

    class TagCategoryColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:

        TagCategoryColumns()
        { add(m_value); add(m_text); }

        Gtk::TreeModelColumn<int32_t> m_value;
        Gtk::TreeModelColumn<Glib::ustring> m_text;
    };

    class TagListColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:

        TagListColumns(){
            
            add(m_id);
            add(m_text);
            add(m_private);
            add(m_aliascount);
            add(m_implycount);
            add(m_used);
        }

        Gtk::TreeModelColumn<int64_t> m_id;
        Gtk::TreeModelColumn<Glib::ustring> m_text;
        Gtk::TreeModelColumn<bool> m_private;
        Gtk::TreeModelColumn<int32_t> m_aliascount;
        Gtk::TreeModelColumn<int32_t> m_implycount;
        Gtk::TreeModelColumn<bool> m_used;
    };
    
public:

    TagManager(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~TagManager();

    //! \brief Updates the list of tags
    void UpdateTagSearch();

    //! \brief Fills in the name field of a new tag
    void SetCreateTag(const std::string &name);

    //! \brief Clears the currently filled in new tag
    void ClearNewTagEntry();
    

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    //! \brief Creates a new tag with the currently set values
    void CreateNewTag();
    
private:

    // Create tag entry widgets
    Gtk::Entry* NewTagName;
    Gtk::Entry* NewTagDescription;
    Gtk::TextView* NewTagAliases;
    Gtk::TextView* NewTagImplies;
    Gtk::CheckButton* NewTagPrivate;
    Gtk::Button* CreateTagButton;

    Gtk::CellRendererText ComboBoxRenderer;
    Gtk::ComboBox* NewTagCategory;

    TagCategoryColumns TagTypeStoreColumns;
    Glib::RefPtr<Gtk::ListStore> TagTypeStore;

    // Existing tag editing
    Gtk::Entry* TagSearch;
    Gtk::TreeView* FoundTags;

    TagListColumns FoundTagStoreColumn;
    Glib::RefPtr<Gtk::ListStore> FoundTagStore;
    
    
};

}

