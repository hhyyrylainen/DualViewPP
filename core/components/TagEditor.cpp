// ------------------------------------ //
#include "TagEditor.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
TagEditor::TagEditor(){
    _UpdateEditable();
}
    
TagEditor::TagEditor(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Box(widget)
{
    _UpdateEditable();
}

TagEditor::~TagEditor(){

    LOG_INFO("TagEditor properly closed");
}
// ------------------------------------ //
void TagEditor::SetEditable(bool editable){

    ShouldBeEditable = editable;
    _UpdateEditable();
}

void TagEditor::_UpdateEditable(){

    if(!ShouldBeEditable || EditedCollections.empty()){

        set_sensitive(false);
        
    } else {

        set_sensitive(true);
    }
}
// ------------------------------------ //
void TagEditor::ReadSetTags(){

    
}

