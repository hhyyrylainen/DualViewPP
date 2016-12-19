// ------------------------------------ //
#include "TagManager.h"

#include "core/resources/Tags.h"
#include "core/UtilityHelpers.h"

#include "Common.h"
#include "core/DualView.h"
#include "core/Database.h"

#include "leviathan/Common/StringOperations.h"
#include "leviathan/Common/ExtraAlgorithms.h"

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

    FoundTags->signal_row_activated().connect(sigc::mem_fun(*this,
            &TagManager::_OnSelectTagToEdit));

    // Tag update widgets
    BUILDER_GET_WIDGET(EditTagName);
    BUILDER_GET_WIDGET(EditTagCategory);
    BUILDER_GET_WIDGET(EditTagIsPrivate);
    BUILDER_GET_WIDGET(EditTagDescription);
    BUILDER_GET_WIDGET(EditTagAliases);
    BUILDER_GET_WIDGET(EditTagImplies);

    // Add model and renderer to the combobox
    EditTagCategory->set_model(TagTypeStore);
    EditTagCategory->set_active(0);
    
    EditTagCategory->pack_start(ComboBoxRenderer);
    EditTagCategory->add_attribute(ComboBoxRenderer, "text", TagTypeStoreColumns.m_text);

    BUILDER_GET_WIDGET(TagEditSave);
    TagEditSave->signal_clicked().connect(sigc::mem_fun(*this, &TagManager::_ApplyTagEdit));
    
    _SetTagEditWidgetsSensitive(false);
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
// ------------------------------------ //
void TagManager::ClearEditedTag(){

    DualView::IsOnMainThreadAssert();

    _SetTagEditWidgetsSensitive(false);
    
    EditedTag = nullptr;
}

void TagManager::_SetTagEditWidgetsSensitive(bool sensitive){

    EditTagName->set_sensitive(sensitive);
    EditTagCategory->set_sensitive(sensitive);
    EditTagIsPrivate->set_sensitive(sensitive);
    EditTagDescription->set_sensitive(sensitive);
    EditTagAliases->set_sensitive(sensitive);
    EditTagImplies->set_sensitive(sensitive);

    TagEditSave->set_sensitive(sensitive);
}

void TagManager::_ReadEditedTagData(){

    if(!EditedTag)
        return;

    EditTagName->set_text(EditedTag->GetName());

    
    NewTagCategory->set_active(0);

    TAG_CATEGORY category = EditedTag->GetCategory();
    
    // Find right one //
    for(auto iter = TagTypeStore->children().begin(); iter != TagTypeStore->children().end();
        ++iter)
    {
        const Gtk::TreeModel::Row& row = *iter;

        const int32_t categoryNumber = row[TagTypeStoreColumns.m_value];

        if(categoryNumber == static_cast<int32_t>(category)){

            // Found the right one //
            EditTagCategory->set_active(iter);
            break;
        }
    }
    
    EditTagIsPrivate->set_active(EditedTag->GetIsPrivate());

    EditTagDescription->get_buffer()->set_text(EditedTag->GetDescription());

    // TODO: these Gets should be on the database thread
    const auto implies = EditedTag->GetImpliedTags();
    
    EditTagAliases->get_buffer()->set_text(Leviathan::StringOperations::StitchTogether<
        std::string>(EditedTag->GetAliases(), "\n"));

    
    std::vector<std::string> implystrings;

    implystrings.reserve(implies.size());

    for(const auto& implytag : implies){

        implystrings.push_back(implytag->GetName());
    }
    
    EditTagImplies->get_buffer()->set_text(Leviathan::StringOperations::StitchTogether<
        std::string>(implystrings, "\n"));
}

void TagManager::_ApplyTagEdit(){

    if(!EditedTag)
        return;

    auto targettag = EditedTag;


    auto rowiter = EditTagCategory->get_active();

    if(!rowiter)
        return;

    const Gtk::TreeModel::Row& row = *rowiter;

    const auto categoryNumber = row[TagTypeStoreColumns.m_value];

    if(categoryNumber < 0 || categoryNumber > static_cast<int>(
            TAG_CATEGORY::QUALITY_HELPFULL_LEVEL))
    {
        LOG_ERROR("Invalid category number when editing tag");
        return;
    }

    // New properties //
    const auto editedname = EditTagName->get_text();
    TAG_CATEGORY category = static_cast<TAG_CATEGORY>(static_cast<int32_t>(categoryNumber));
    std::vector<std::string> editedimplies;
    std::vector<std::string> editedaliases;
    const auto editedisprivate = EditTagIsPrivate->get_active();
    const auto editeddescription = EditTagDescription->get_buffer()->get_text();

    Leviathan::StringOperations::CutLines<std::string>(
        EditTagAliases->get_buffer()->get_text(), editedaliases);


    Leviathan::StringOperations::CutLines<std::string>(
        EditTagImplies->get_buffer()->get_text(), editedimplies);

    // Disable editing //
    _SetTagEditWidgetsSensitive(false);

    auto isalive = GetAliveMarker();

    // Called if changing stuff fails //
    auto onfail = [=](const std::string &message){

        DualView::Get().InvokeFunction([=](){

                INVOKE_CHECK_ALIVE_MARKER(isalive);

                auto dialog = Gtk::MessageDialog(*this,
                    "Failed to apply tag changes",
                    false,
                    Gtk::MESSAGE_ERROR,
                    Gtk::BUTTONS_OK,
                    true 
                );

                dialog.set_secondary_text("Error applying changes to tag id:" +
                    Convert::ToString(targettag->GetID()) + " \"" + targettag->GetName() +
                    "\" error: " + message);
                
                dialog.run();

                // Don't mess with things if the EditedTag has been changed
                if(EditedTag != targettag)
                    return;

                _ReadEditedTagData();

                _SetTagEditWidgetsSensitive(true);
            });
    };
    
    DualView::Get().QueueDBThreadFunction([=](){

            try{
                // Apply new things //
                if(targettag->GetName() != editedname)
                    targettag->SetName(editedname);

                if(targettag->GetCategory() != category)
                    targettag->SetCategory(category);

                if(targettag->GetDescription() != editeddescription)
                    targettag->SetDescription(editeddescription);

                if(targettag->GetIsPrivate() != editedisprivate)
                    targettag->SetIsPrivate(editedisprivate);

                // Apply changes to aliases //
                std::vector<std::string> added;
                std::vector<std::string> removed;

                Leviathan::FindRemovedElements(targettag->GetAliases(),
                    editedaliases, added, removed);

                for(const auto& value : added){

                    targettag->AddAlias(value);
                }

                for(const auto& value : removed){

                    targettag->RemoveAlias(value);
                }

                targettag->Save();

                added.clear();
                removed.clear();

                const auto implies = EditedTag->GetImpliedTags();
                std::vector<std::string> implystrings;

                implystrings.reserve(implies.size());

                for(const auto& implytag : implies){

                    implystrings.push_back(implytag->GetName());
                }

                Leviathan::FindRemovedElements(implystrings,
                    editedimplies, added, removed);

                for(const auto& value : added){

                    targettag->AddImpliedTag(
                        DualView::Get().GetDatabase().SelectTagByNameOrAlias(value));
                }

                for(const auto& value : removed){

                    targettag->RemoveImpliedTag(
                        DualView::Get().GetDatabase().SelectTagByNameOrAlias(value));
                }

            } catch(const InvalidSQL &e){

                LOG_ERROR("TagManager: tag update failed, sql error: " +
                    std::string(e.what()));
                onfail(e.what());
                return;
            }

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    // Don't mess with things if the EditedTag has been changed
                    if(EditedTag != targettag)
                        return;

                    _ReadEditedTagData();

                    _SetTagEditWidgetsSensitive(true);
                });
        });
}

void TagManager::_OnSelectTagToEdit(const Gtk::TreeModel::Path& path,
    Gtk::TreeViewColumn* column)
{
    const Gtk::TreeModel::Row& row = *FoundTagStore->get_iter(path);

    const int64_t tagid = row[FoundTagStoreColumn.m_id];

    if(EditedTag && tagid == EditedTag->GetID())
        return;

    ClearEditedTag();
    
    auto isalive = GetAliveMarker();
    
    DualView::Get().QueueDBThreadFunction([=](){

            auto newtag = DualView::Get().GetDatabase().SelectTagByIDAG(tagid);

            if(!newtag){

                LOG_ERROR("TagManager: failed to find tag by id to edit");
                return;
            }

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    EditedTag = newtag;

                    _ReadEditedTagData();

                    _SetTagEditWidgetsSensitive(true);
                });
        });
}


