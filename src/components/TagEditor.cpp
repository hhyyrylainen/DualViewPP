// ------------------------------------ //
#include "TagEditor.h"

#include "Common.h"
#include "DualView.h"

#include "Exceptions.h"

#include <canberra-gtk.h>

using namespace DV;
// ------------------------------------ //

TagEditor::TagEditor() :
    Gtk::Box(Gtk::ORIENTATION_VERTICAL), ContainerForTags(),
    ViewForTags(ContainerForTags.get_hadjustment(), ContainerForTags.get_vadjustment())
{
    _CommonCtor();
}

TagEditor::TagEditor(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Box(widget), ContainerForTags(),
    ViewForTags(ContainerForTags.get_hadjustment(), ContainerForTags.get_vadjustment())
{
    _CommonCtor();
}

void TagEditor::_CommonCtor()
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    CreateTag.set_image_from_icon_name("document-new-symbolic");

    set_spacing(2);

    // This doesn't seem to work if the container is not set as expand
    // in the .glade layout...
    set_hexpand(true);

    add(Title);
    Title.set_text("Tag Editor");

    // Container for tree
    add(ContainerForTags);
    ContainerForTags.add(ViewForTags);

    child_property_expand(ContainerForTags) = true;

    // Set the shown columns
    TagsTreeView.append_column("Tag Full Name", TreeViewColumns.m_tag_as_text);
    TagsTreeView.append_column("Set Count", TreeViewColumns.m_in_how_many_containers);

    TagsTreeView.add_events(Gdk::KEY_PRESS_MASK);

    TagsTreeView.signal_key_press_event().connect(
        sigc::mem_fun(*this, &TagEditor::_OnKeyPress));


    // add_events(Gdk::BUTTON_PRESS_MASK);

    signal_button_press_event().connect(sigc::mem_fun(*this, &TagEditor::_RowClicked));

    TagsTreeView.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);

    ViewForTags.add(TagsTreeView);

    auto* textcolumn = TagsTreeView.get_column(0);
    textcolumn->set_expand(true);

    // Expand set this way to stop this container from also expanding
    child_property_expand(TagsTreeView) = true;

    // Add an EntryCompletion:
    // Setup auto complete
    TagEntryCompletion.Init(&TagEntry,
        std::bind(&TagEditor::_OnSuggestionSelected, this, std::placeholders::_1),
        std::bind(&DualView::GetSuggestionsForTag, &DualView::Get(), std::placeholders::_1,
            std::placeholders::_2));

    TagEntry.set_placeholder_text("input new tag here");
    TagEntry.signal_activate().connect(sigc::mem_fun(*this, &TagEditor::_OnInsertTag));

    add(TagEntry);

    CreateTag.set_always_show_image();
    CreateTag.signal_clicked().connect(sigc::mem_fun(*this, &TagEditor::_OnCreateNew));
    add(CreateTag);

    show_all_children();
    _UpdateEditable();
}

TagEditor::~TagEditor()
{
    LOG_INFO("TagEditor properly closed");
}
// ------------------------------------ //
void TagEditor::SetEditable(bool editable)
{
    ShouldBeEditable = editable;
    _UpdateEditable();
}

void TagEditor::_UpdateEditable()
{
    // Update the title //
    Title.set_text("Tag Editor (" + Convert::ToString(EditedCollections.size()) + ")");

    if(!ShouldBeEditable || EditedCollections.empty()) {

        set_sensitive(false);

    } else {

        set_sensitive(true);
    }
}
// ------------------------------------ //
void TagEditor::ReadSetTags()
{
    std::vector<std::tuple<AppliedTag* const, int>> tags;

    for(const auto& collection : EditedCollections) {

        // This should force tags to load
        if(!collection->HasTags())
            continue;

        for(const auto& settag : *collection) {

            AppliedTag* const tag = settag.get();

            bool set = false;

            // Check is it already set //
            for(auto& alreadyset : tags) {

                if(std::get<0>(alreadyset)->IsSame(*tag)) {

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

    // Fill the TreeView's model
    for(const auto& tag : tags) {

        Gtk::TreeModel::Row row = *(TagsModel->append());
        row[TreeViewColumns.m_tag_as_text] = std::get<0>(tag)->ToAccurateString();
        row[TreeViewColumns.m_in_how_many_containers] = std::get<1>(tag);
    }
}
// ------------------------------------ //
bool TagEditor::AddTag(const std::string tagstr)
{
    // Parse tag //
    std::shared_ptr<AppliedTag> tag;

    try {

        tag = DualView::Get().ParseTagFromString(tagstr);

    } catch(const Leviathan::InvalidArgument& e) {

        LOG_INFO("TagEditor: unknown tag '" + tagstr + "': ");
        e.PrintToLog();
        return false;
    }

    if(!tag)
        return false;

    for(const auto& collection : EditedCollections) {

        collection->Add(tag);
    }

    ReadSetTags();
    return true;
}

void TagEditor::DeleteTag(const std::string tagstr)
{
    for(const auto& collection : EditedCollections) {

        collection->RemoveText(tagstr);
    }

    ReadSetTags();
}
// ------------------------------------ //
void TagEditor::_OnInsertTag()
{
    const auto text = TagEntry.get_text();

    if(text == "")
        return;

    if(AddTag(text)) {

        TagEntry.set_text("");

    } else {

        // Invalid tag //
        // Play a sound from XDG Sound Naming Specification
        ca_gtk_play_for_widget(
            ((Gtk::Widget*)this)->gobj(), 0, CA_PROP_EVENT_ID, "dialog-error", NULL);
    }
}

void TagEditor::_OnCreateNew()
{
    DualView::Get().OpenTagCreator(TagEntry.get_text());
}

bool TagEditor::_OnKeyPress(GdkEventKey* key_event)
{
    if(key_event->type == GDK_KEY_PRESS) {

        switch(key_event->keyval) {
        case GDK_KEY_Delete: {
            std::vector<Glib::ustring> todelete;
            auto selected = TagsTreeView.get_selection()->get_selected_rows();

            for(const auto& path : selected) {

                Gtk::TreeModel::Row row = *(TagsModel->get_iter(path));
                todelete.push_back(row[TreeViewColumns.m_tag_as_text]);
            }

            LOG_INFO("TagEditor: deleting " + Convert::ToString(todelete.size()) + " tags");

            for(const auto& str : todelete) {

                DeleteTag(str);
            }

            return true;
        }
        }
    }

    return false;
}

bool TagEditor::_RowClicked(GdkEventButton* event)
{
    // Double click with left mouse //
    if(event->type == GDK_2BUTTON_PRESS && event->button == 1) {

        auto selected = TagsTreeView.get_selection()->get_selected_rows();

        for(const auto& path : selected) {

            Gtk::TreeModel::Row row = *(TagsModel->get_iter(path));
            const auto tag = row[TreeViewColumns.m_tag_as_text];

            LOG_INFO("Viewing Tag info for: " + tag);

            DualView::Get().OpenTagInfo(static_cast<Glib::ustring>(tag));
            return true;
        }

        return false;
    }

    return false;
}
// ------------------------------------ //
bool TagEditor::_OnSuggestionSelected(const Glib::ustring& str)
{
    if(AddTag(str)) {

        return true;
    }

    return false;
}
