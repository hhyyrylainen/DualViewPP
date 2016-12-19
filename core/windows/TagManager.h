#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class Tag;

//! \brief Window that shows all the tags and allows editing them
class TagManager : public Gtk::Window, public IsAlive{

    class TagCategoryColumns : public Gtk::TreeModel::ColumnRecord{
    public:

        TagCategoryColumns()
        { add(m_value); add(m_text); }

        Gtk::TreeModelColumn<int32_t> m_value;
        Gtk::TreeModelColumn<Glib::ustring> m_text;
    };

    class TagListColumns : public Gtk::TreeModel::ColumnRecord{
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

    //! \brief Sets the string to search with for tags
    void SetSearchString(const std::string &text);

    //! \brief Fills in the name field of a new tag
    void SetCreateTag(const std::string &name);

    //! \brief Clears the currently filled in new tag
    void ClearNewTagEntry();

    //! \brief Resets the edited tag
    void ClearEditedTag();

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    //! Copies text from new tag entry to the search field
    void _NewTagChanged();

    //! \brief Creates a new tag with the currently set values
    void CreateNewTag();

    //! \brief Sets the currently selected tag as the one to be edited
    void _OnSelectTagToEdit(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);

    void _SetTagEditWidgetsSensitive(bool sensitive);

    //! \brief Reads tag data into edit widgets
    void _ReadEditedTagData();

    //! \brief Applies changed Tag things
    void _ApplyTagEdit();
    
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

    // Tag update widgets
    Gtk::Entry* EditTagName;
    Gtk::ComboBox* EditTagCategory;
    Gtk::CheckButton* EditTagIsPrivate;
    Gtk::TextView* EditTagDescription;
    Gtk::TextView* EditTagAliases;
    Gtk::TextView* EditTagImplies;

    Gtk::Button* TagEditSave;
        
    std::shared_ptr<Tag> EditedTag;

    // Existing tag editing
    Gtk::SearchEntry* TagSearch;
    Gtk::TreeView* FoundTags;

    TagListColumns FoundTagStoreColumn;
    Glib::RefPtr<Gtk::ListStore> FoundTagStore;
    
    
};

}

