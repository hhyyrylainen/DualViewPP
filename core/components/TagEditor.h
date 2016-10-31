#pragma once

#include "core/resources/Tags.h"

#include <gtkmm.h>

namespace DV{

class TagEditor : public Gtk::Box{

    class ModelColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:

        ModelColumns()
        { add(m_tag_as_text); add(m_in_how_many_containers); }

        Gtk::TreeModelColumn<Glib::ustring> m_tag_as_text;
        Gtk::TreeModelColumn<int> m_in_how_many_containers;
    };

    class TagCompletionColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:

        TagCompletionColumns()
        { add(m_tag_text); }

        Gtk::TreeModelColumn<Glib::ustring> m_tag_text;
    };

public:

    //! \brief Non-glade constructor
    TagEditor();
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    TagEditor(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~TagEditor();

    //! \brief Sets the collections to edit
    void SetEditedTags(const std::vector<std::shared_ptr<TagCollection>> &tagstoedit){

        EditedCollections = tagstoedit;
        
        ReadSetTags();
        _UpdateEditable();
    }

    //! \brief Adds a tag
    //! \returns True if succeeded, false if the tag string wasn't valid
    //! \todo Make this use the database thread to avoid user visible lag
    bool AddTag(const std::string tagstr);

    //! \brief Removes a tag
    void DeleteTag(const std::string tagstr);

    //! \brief Sets this editable
    //!
    //! If there is nothing in EditedCollections this will always set this to not sensitive
    void SetEditable(bool editable);

    //! \brief Reads currently set tags from the TagCollections
    void ReadSetTags();

protected:

    //! Sets up child widgets
    void _CommonCtor();

    //! \brief Updates the actual editability of this
    void _UpdateEditable();

    // Gtk callbacks //
    void _OnInsertTag();
    void _OnCreateNew();
    //! Detects when delete is pressed
    bool _OnKeyPress(GdkEventKey* key_event);

protected:

    // Gtk widgets //
    Gtk::Label Title;

    // New tag button //
    Gtk::Button CreateTag;

    // Container for the set tags view
    

    // List of active tags //
    Gtk::TreeView TagsTreeView;
    Glib::RefPtr<Gtk::ListStore> TagsModel;
    ModelColumns TreeViewColumns;

    // Tag entry Gtk::Entry and auto completion
    Gtk::Entry TagEntry;
    Glib::RefPtr<Gtk::EntryCompletion> TagCompletion;
    TagCompletionColumns CompletionColumns;


    //! This is directly set by SetEditable. Contains the value our owner wants
    bool ShouldBeEditable = true;
    
    //! Here are all the tags stored
    std::vector<std::shared_ptr<TagCollection>> EditedCollections;
};

}
