// ------------------------------------ //
#include "TagManager.h"

#include "core/resources/Tags.h"

#include "Common.h"
#include "core/DualView.h"
#include "core/Database.h"

using namespace DV;
// ------------------------------------ //
TagManager::TagManager(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &TagManager::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &TagManager::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &TagManager::_OnShown));
    
    builder->get_widget("NewTagName", NewTagName);
    LEVIATHAN_ASSERT(NewTagName, "Invalid .glade file");

    builder->get_widget("NewTagCategory", NewTagCategory);
    LEVIATHAN_ASSERT(NewTagCategory, "Invalid .glade file");
    
    builder->get_widget("NewTagDescription", NewTagDescription);
    LEVIATHAN_ASSERT(NewTagDescription, "Invalid .glade file");
    
    builder->get_widget("NewTagAliases", NewTagAliases);
    LEVIATHAN_ASSERT(NewTagAliases, "Invalid .glade file");

    builder->get_widget("NewTagImplies", NewTagImplies);
    LEVIATHAN_ASSERT(NewTagImplies, "Invalid .glade file");

    builder->get_widget("NewTagPrivate", NewTagPrivate);
    LEVIATHAN_ASSERT(NewTagPrivate, "Invalid .glade file");
    
    builder->get_widget("CreateTagButton", CreateTagButton);
    LEVIATHAN_ASSERT(CreateTagButton, "Invalid .glade file");

    CreateTagButton->signal_clicked().connect(sigc::mem_fun(*this, &TagManager::CreateNewTag));

    TagTypeStore = Gtk::ListStore::create(TagTypeStoreColumns);

    // Fill values //
    for(const auto &tuple : TAG_CATEGORY_STR){

        Gtk::TreeModel::Row row = *(TagTypeStore->append());
        row[TagTypeStoreColumns.m_value] = static_cast<int32_t>(std::get<0>(tuple));
        row[TagTypeStoreColumns.m_text] = std::get<1>(tuple);
    }

    NewTagCategory->set_model(TagTypeStore);
    NewTagCategory->set_active(0);

    // Add renderer
    NewTagCategory->pack_start(ComboBoxRenderer);
    NewTagCategory->add_attribute(ComboBoxRenderer, "text", TagTypeStoreColumns.m_text);


    builder->get_widget("TagSearch", TagSearch);
    LEVIATHAN_ASSERT(TagSearch, "Invalid .glade file");

    builder->get_widget("FoundTags", FoundTags);
    LEVIATHAN_ASSERT(FoundTags, "Invalid .glade file");
    
    FoundTagStore = Gtk::ListStore::create(FoundTagStoreColumn);

    FoundTags->set_model(FoundTagStore);

    FoundTags->append_column("ID", FoundTagStoreColumn.m_id);
    FoundTags->append_column("As Text", FoundTagStoreColumn.m_text);
    FoundTags->append_column("Private", FoundTagStoreColumn.m_private);
    FoundTags->append_column("# Aliases", FoundTagStoreColumn.m_aliascount);
    FoundTags->append_column("# Implies", FoundTagStoreColumn.m_implycount);
    FoundTags->append_column("Used", FoundTagStoreColumn.m_used);

    auto* textcolumn = FoundTags->get_column(1);
    textcolumn->set_expand(true);
    textcolumn->set_sort_column(FoundTagStoreColumn.m_text);
}

TagManager::~TagManager(){

}
// ------------------------------------ //
bool TagManager::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();

    return true;
}
// ------------------------------------ //
void TagManager::_OnShown(){

    // Load items //
    UpdateTagSearch();
}

void TagManager::_OnHidden(){

}
// ------------------------------------ //
void TagManager::UpdateTagSearch(){

    auto isalive = GetAliveMarker();
    std::string search = "";
    
    DualView::Get().QueueDBThreadFunction([=](){

            auto tags = DualView::Get().GetDatabase().SelectTagsWildcard(search, 100);
            
            DualView::Get().InvokeFunction([this, isalive, tags{std::move(tags)}](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    for(const auto& tag : tags){

                        Gtk::TreeModel::Row row = *(FoundTagStore->append());
                        row[FoundTagStoreColumn.m_id] = tag->GetID();
                        row[FoundTagStoreColumn.m_text] = tag->GetName();
                        row[FoundTagStoreColumn.m_private] = tag->GetIsPrivate();
                    }
                });
        });
}


// ------------------------------------ //
void TagManager::SetCreateTag(const std::string &name){
    
    ClearNewTagEntry();

    NewTagName->set_text(name);
}

void TagManager::ClearNewTagEntry(){

    NewTagCategory->set_active(0);
    NewTagName->set_text("");
    NewTagDescription->set_text("");
    NewTagImplies->get_buffer()->set_text("");
    NewTagAliases->get_buffer()->set_text("");
}

// ------------------------------------ //
void TagManager::CreateNewTag(){

    auto rowiter = NewTagCategory->get_active();

    if(!rowiter)
        return;

    const Gtk::TreeModel::Row& row = *rowiter;

    auto model = NewTagCategory->get_model();

    const auto categoryNumber = row[TagTypeStoreColumns.m_value];

    if(categoryNumber < 0 || categoryNumber > static_cast<int>(
            TAG_CATEGORY::QUALITY_HELPFULL_LEVEL))
    {
        LOG_ERROR("Invalid category number when creating tag");
        return;
    }

    std::string name = NewTagName->get_text();
    std::string description = NewTagDescription->get_text();
    TAG_CATEGORY category = static_cast<TAG_CATEGORY>(static_cast<int32_t>(categoryNumber));
    bool isprivate = NewTagPrivate->get_active();

    auto isalive = GetAliveMarker();
    
    DualView::Get().QueueDBThreadFunction([=](){

            try{
                
                auto tag = DualView::Get().GetDatabase().InsertTag(name, description,
                    category, isprivate);

                if(!tag)
                    throw InvalidSQL("got null back from database", 1, "");
                
            } catch(const InvalidSQL &e){

                LOG_ERROR("Failed to create new tag: " + std::string(e.what()));
                return;
            }

            LOG_WRITE("TODO: implies");
            
            DualView::Get().InvokeFunction([this, isalive](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    ClearNewTagEntry();
                    UpdateTagSearch();
                });
        });
}

