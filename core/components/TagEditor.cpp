// ------------------------------------ //
#include "TagEditor.h"

#include "Common.h"
#include "DualView.h"

#include "Exceptions.h"

#include <canberra-gtk.h>

using namespace DV;
// ------------------------------------ //

TagEditor::TagEditor() : Gtk::Box(Gtk::ORIENTATION_VERTICAL),
    CreateTag(Gtk::StockID("gtk-add"))
{

    _CommonCtor();
}
    
TagEditor::TagEditor(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Box(widget),
    CreateTag(Gtk::StockID("gtk-add"))
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    _CommonCtor();
}

void TagEditor::_CommonCtor(){

    set_spacing(2);

    add(Title);
    Title.set_text("Tag Editor");
    
    // Set the shown columns
    TagsTreeView.append_column("Tag Full Name", TreeViewColumns.m_tag_as_text);
    TagsTreeView.append_column("Set Count", TreeViewColumns.m_in_how_many_containers);

    add(TagsTreeView);
    // Expand set this way to stop this container from also expanding
    child_property_expand(TagsTreeView) = true;


    //Add an EntryCompletion:
    TagCompletion = Gtk::EntryCompletion::create();
    TagEntry.set_completion(TagCompletion);

    //Create and fill the completion's filter model
    auto refCompletionModel =
        Gtk::ListStore::create(CompletionColumns);
    TagCompletion->set_model(refCompletionModel);

    // For more complex comparisons, use a filter match callback, like this.
    // See the comment below for more details:
    //completion->set_match_func( sigc::mem_fun(*this,
    //&ExampleWindow::on_completion_match) );

    //Fill the TreeView's model
    Gtk::TreeModel::Row row = *(refCompletionModel->append());
    row[CompletionColumns.m_tag_text] = "Outen";

    row = *(refCompletionModel->append());
    row[CompletionColumns.m_tag_text] = "Outer ring";

    row = *(refCompletionModel->append());
    row[CompletionColumns.m_tag_text] = "Outside";

    row = *(refCompletionModel->append());
    row[CompletionColumns.m_tag_text] = "Outdoors";

    row = *(refCompletionModel->append());
    row[CompletionColumns.m_tag_text] = "Outdoors type";

    //Tell the completion what model column to use to
    //- look for a match (when we use the default matching, instead of
    //  set_match_func().
    //- display text in the entry when a match is found.
    TagCompletion->set_text_column(CompletionColumns.m_tag_text);

    // Doesn't seem to work
    //TagCompletion->set_inline_completion();
    TagCompletion->set_inline_selection();

    TagEntry.set_placeholder_text("input new tag here");
    TagEntry.signal_activate().connect(sigc::mem_fun(*this, &TagEditor::_OnInsertTag));
    
    add(TagEntry);

    CreateTag.set_always_show_image();
    CreateTag.signal_clicked().connect(sigc::mem_fun(*this, &TagEditor::_OnCreateNew));
    add(CreateTag);

    show_all_children();
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

    // Update the title //
    Title.set_text("Tag Editor (" + Convert::ToString(EditedCollections.size()) + ")");
    
    if(!ShouldBeEditable || EditedCollections.empty()){

        set_sensitive(false);
        
    } else {

        set_sensitive(true);
    }
}
// ------------------------------------ //
void TagEditor::ReadSetTags(){

    std::vector<std::tuple<AppliedTag* const, int>> tags;

    for(const auto& collection : EditedCollections){

        for(const auto& settag : *collection){

            AppliedTag* const tag = settag.get();

            bool set = false;
            
            // Check is it already set //
            for(auto& alreadyset : tags){

                if(std::get<0>(alreadyset)->IsSame(*tag)){

                    set = true;
                    ++std::get<1>(alreadyset);
                    break;
                }
            }

            if(set)
                continue;

            tags.push_back(std::make_tuple(tag, 1));
        }
    }

    // Setup tree //
    TagsModel = Gtk::ListStore::create(TreeViewColumns);
    TagsTreeView.set_model(TagsModel);

    //Fill the TreeView's model
    for(const auto& tag : tags){
        
        Gtk::TreeModel::Row row = *(TagsModel->append());
        row[TreeViewColumns.m_tag_as_text] = std::get<0>(tag)->ToAccurateString();
        row[TreeViewColumns.m_in_how_many_containers] = std::get<1>(tag);
    }
}
// ------------------------------------ //
bool TagEditor::AddTag(const std::string tagstr){

    // Parse tag //
    std::shared_ptr<AppliedTag> tag;

    try{

        tag = DualView::Get().ParseTagFromString(tagstr);

    } catch(const Leviathan::InvalidArgument &e){

        LOG_INFO("TagEditor: unknown tag '" + tagstr + "': ");
        e.PrintToLog();
        return false;
    }

    if(!tag)
        return false;

    for(const auto& collection : EditedCollections){

        collection->Add(tag);
    }
    
    ReadSetTags();
    return true;
}
// ------------------------------------ //
void TagEditor::_OnInsertTag(){

    const auto text = TagEntry.get_text();

    if(text == "")
        return;

    if(AddTag(text)){

        TagEntry.set_text("");
        
    } else {

        // Invalid tag //
        // Play a sound from XDG Sound Naming Specification
        ca_gtk_play_for_widget(((Gtk::Widget*)this)->gobj(), 0,
            CA_PROP_EVENT_ID, "dialog-error",
            NULL);
    }
}

void TagEditor::_OnCreateNew(){

    DualView::Get().OpenTagCreator(TagEntry.get_text());
}
