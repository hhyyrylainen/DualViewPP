// ------------------------------------ //
#include "TagManager.h"

#include "core/resources/Tags.h"
#include "core/UtilityHelpers.h"

#include "Common.h"
#include "core/DualView.h"
#include "core/Database.h"

#include "leviathan/Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //
TagManager::TagManager(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &TagManager::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &TagManager::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &TagManager::_OnShown));
    
    BUILDER_GET_WIDGET(NewTagName);

    NewTagName->signal_changed().connect(sigc::mem_fun(*this, &TagManager::_NewTagChanged));

    BUILDER_GET_WIDGET(NewTagCategory);
    
    BUILDER_GET_WIDGET(NewTagDescription);
    
    BUILDER_GET_WIDGET(NewTagAliases);

    BUILDER_GET_WIDGET(NewTagImplies);

    BUILDER_GET_WIDGET(NewTagPrivate);
    
    BUILDER_GET_WIDGET(CreateTagButton);

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


    BUILDER_GET_WIDGET(TagSearch);

    TagSearch->signal_search_changed().connect(sigc::mem_fun(*this,
            &TagManager::UpdateTagSearch));

    BUILDER_GET_WIDGET(FoundTags);
    
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

    // Load items, but only if not already loaded //
    if(FoundTagStore->children().empty())
        UpdateTagSearch();
}

void TagManager::_OnHidden(){

}
// ------------------------------------ //
void TagManager::UpdateTagSearch(){

    auto isalive = GetAliveMarker();
    std::string search = TagSearch->get_text();
    
    DualView::Get().QueueDBThreadFunction([=](){

            auto tags = DualView::Get().GetDatabase().SelectTagsWildcard(search, 100);

            SortTagSuggestions(tags.begin(), tags.end(), search);
            
            DualView::Get().InvokeFunction([this, isalive, tags{std::move(tags)}](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    FoundTagStore->clear();

                    for(const auto& tag : tags){

                        Gtk::TreeModel::Row row = *(FoundTagStore->append());
                        row[FoundTagStoreColumn.m_id] = tag->GetID();
                        row[FoundTagStoreColumn.m_text] = tag->GetName();
                        row[FoundTagStoreColumn.m_private] = tag->GetIsPrivate();
                    }
                });
        });
}

void TagManager::SetSearchString(const std::string &text){

    TagSearch->set_text(text);

    // The update to the text causes UpdateTagSearch to be called
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

void TagManager::_NewTagChanged(){

    if(!NewTagName->get_text().empty()){

        TagSearch->set_text(NewTagName->get_text());
    }
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

    std::vector<std::string> newaliases;
    Leviathan::StringOperations::CutLines<std::string>(
        NewTagAliases->get_buffer()->get_text(), newaliases);

    std::vector<std::string> newimplies;
    Leviathan::StringOperations::CutLines<std::string>(
        NewTagImplies->get_buffer()->get_text(), newimplies);

    auto isalive = GetAliveMarker();
    
    DualView::Get().QueueDBThreadFunction([=](){

            // Find implies //
            std::vector<std::shared_ptr<Tag>> implytags;

            for(const auto& imply : newimplies){

                auto foundtag = DualView::Get().GetDatabase().SelectTagByNameOrAlias(imply);

                if(!foundtag){

                    LOG_ERROR("Failed to create new tag, because implied tag doesn't exist: "
                        + imply);
                    return;
                }

                implytags.push_back(foundtag);
            }

            try{
                
                auto tag = DualView::Get().GetDatabase().InsertTag(name, description,
                    category, isprivate);

                if(!tag)
                    throw InvalidSQL("got null back from database", 1, "");

                // Aliases //
                for(const auto& alias : newaliases)
                    tag->AddAlias(alias);
                
                // Implies //
                for(const auto& imply : implytags)
                    tag->AddImpliedTag(imply);
                
            } catch(const InvalidSQL &e){

                LOG_ERROR("Failed to create new tag: " + std::string(e.what()));
                return;
            }
            
            DualView::Get().InvokeFunction([this, isalive](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    ClearNewTagEntry();
                    UpdateTagSearch();
                });
        });
}

