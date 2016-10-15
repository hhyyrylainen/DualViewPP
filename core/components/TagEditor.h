#pragma once

#include "core/resources/Tags.h"

#include <gtkmm.h>

namespace DV{

class TagEditor : public Gtk::Box{
public:

    //! \brief Non-glade constructor
    TagEditor();
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    TagEditor(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~TagEditor();

    //! \brief Sets the collections to edit
    void SetEditedTags(std::vector<std::shared_ptr<TagCollection>> &tagstoedit){

        EditedCollections = tagstoedit;
        
        ReadSetTags();
        _UpdateEditable();
    }

    //! \brief Sets this editable
    //!
    //! If there is nothing in EditedCollections this will always set this to not sensitive
    void SetEditable(bool editable);

    //! \brief Reads currently set tags from the TagCollections
    void ReadSetTags();

protected:

    //! \brief Updates the actual editability of this
    void _UpdateEditable();

protected:

    // Gtk widgets //


    //! This is directly set by SetEditable. Contains the value our owner wants
    bool ShouldBeEditable = true;
    
    //! Here are all the tags stored
    std::vector<std::shared_ptr<TagCollection>> EditedCollections;
};

}
