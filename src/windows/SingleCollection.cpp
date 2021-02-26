// ------------------------------------ //
#include "SingleCollection.h"

#include "resources/Collection.h"
#include "resources/DatabaseAction.h"

#include "components/ImageListItem.h"
#include "components/SuperContainer.h"
#include "components/TagEditor.h"

#include "Common.h"
#include "Database.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
SingleCollection::SingleCollection(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    builder->get_widget_derived("ImageContainer", ImageContainer);
    LEVIATHAN_ASSERT(ImageContainer, "Invalid .glade file");

    builder->get_widget_derived("CollectionTags", CollectionTags);
    LEVIATHAN_ASSERT(CollectionTags, "Invalid .glade file");
    CollectionTags->hide();

    builder->get_widget("OpenTagEdit", OpenTagEdit);
    LEVIATHAN_ASSERT(OpenTagEdit, "Invalid .glade file");

    builder->get_widget("StatusLabel", StatusLabel);
    LEVIATHAN_ASSERT(StatusLabel, "Invalid .glade file");

    OpenTagEdit->signal_clicked().connect(
        sigc::mem_fun(*this, &SingleCollection::ToggleTagEditor));

    BUILDER_GET_WIDGET(DeleteSelected);

    DeleteSelected->signal_clicked().connect(
        sigc::mem_fun(*this, &SingleCollection::_OnDeleteSelected));

    BUILDER_GET_WIDGET(OpenSelectedImporter);

    OpenSelectedImporter->signal_clicked().connect(
        sigc::mem_fun(*this, &SingleCollection::_OnOpenSelectedInImporter));

    BUILDER_GET_WIDGET(DeleteThisCollection);

    DeleteThisCollection->signal_clicked().connect(
        sigc::mem_fun(*this, &SingleCollection::_OnDeleteRestorePressed));

    Gtk::ToolButton* rename;
    BUILDER_GET_WIDGET_NAMED(rename, "Rename");

    rename->signal_clicked().connect(sigc::mem_fun(*this, &SingleCollection::StartRename));

    BUILDER_GET_WIDGET(ReorderThisCollection);

    ReorderThisCollection->signal_clicked().connect(
        sigc::mem_fun(*this, &SingleCollection::Reorder));

    _UpdateDeletedStatus();
}

SingleCollection::~SingleCollection()
{
    Close();

    LOG_INFO("SingleCollection window destructed");
}
// ------------------------------------ //
void SingleCollection::ShowCollection(std::shared_ptr<Collection> collection)
{
    // Detach old collection, if there is one //
    GUARD_LOCK();

    ReleaseParentHooks(guard);
    ShownCollection = collection;

    _UpdateDeletedStatus();

    ReloadImages(guard);
}
// ------------------------------------ //
void SingleCollection::OnNotified(
    Lock& ownlock, Leviathan::BaseNotifierAll* parent, Lock& parentlock)
{
    _UpdateDeletedStatus();
    ReloadImages(ownlock);
}

void SingleCollection::ReloadImages(Lock& guard)
{
    // Start listening for changes in the collection //
    if(ShownCollection)
        if(!IsConnectedTo(ShownCollection.get(), guard))
            ConnectToNotifier(guard, ShownCollection.get());

    StatusLabel->set_text("Loading Collection...");

    if(ShownCollection) {
        set_title(ShownCollection->GetName() + " - " +
                  (ShownCollection->IsDeleted() ? "DELETED " : "") +
                  "Collection - DualView++");
    } else {
        set_title("None - Collection - DualView++");
    }

    if(CollectionTags->get_visible()) {

        // Update tags //
        CollectionTags->SetEditedTags(
            {ShownCollection ? ShownCollection->GetTags() : nullptr});
    }

    std::shared_ptr<Collection> collection = ShownCollection;
    if(!collection)
        return;

    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([this, isalive, collection]() {
        auto images = collection->GetImages();

        DualView::Get().InvokeFunction([this, isalive, images, collection]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            ImageContainer->SetShownItems(images.begin(), images.end(),
                std::make_shared<ItemSelectable>([=](ListItem& item) {
                    bool hasselected = ImageContainer->CountSelectedItems() > 0;

                    // Enable buttons //
                    DeleteSelected->set_sensitive(hasselected);
                    OpenSelectedImporter->set_sensitive(hasselected);
                }));

            // This is probably really innefficient //
            ImageContainer->VisitAllWidgets([&](ListItem& widget) {
                auto* asimage = dynamic_cast<ImageListItem*>(&widget);

                if(!asimage)
                    return;

                asimage->SetCollection(collection);
            });

            StatusLabel->set_text(
                "Collection \"" + collection->GetName() + "\" Has " +
                Convert::ToString(images.size()) + " Images" +
                (ShownCollection->IsDeleted() ? ". This collection is DELETED!" : ""));
        });
    });
}
// ------------------------------------ //
void SingleCollection::StartRename()
{
    DualView::Get().OpenCollectionRename(ShownCollection, this);
}
// ------------------------------------ //
void SingleCollection::Reorder()
{
    DualView::Get().OpenReorder(ShownCollection);
}
// ------------------------------------ //
void SingleCollection::_OnClose() {}
// ------------------------------------ //
void SingleCollection::ToggleTagEditor()
{
    if(CollectionTags->get_visible()) {

        CollectionTags->SetEditedTags({});
        CollectionTags->hide();

    } else {

        CollectionTags->show();
        CollectionTags->SetEditedTags(
            {ShownCollection ? ShownCollection->GetTags() : nullptr});
    }
}
// ------------------------------------ //
std::vector<std::shared_ptr<Image>> SingleCollection::GetSelected() const
{
    std::vector<std::shared_ptr<ResourceWithPreview>> items;
    ImageContainer->GetSelectedItems(items);

    std::vector<std::shared_ptr<Image>> imgs;
    imgs.reserve(items.size());

    for(const auto& i : items) {

        auto casted = std::dynamic_pointer_cast<Image>(i);

        if(casted)
            imgs.push_back(casted);
    }

    return imgs;
}

void SingleCollection::_OnDeleteSelected()
{
    auto isalive = GetAliveMarker();
    const auto images = GetSelected();
    const auto collection = ShownCollection;

    if(!collection)
        return;

    DualView::Get().QueueDBThreadFunction([=]() {
        DualView::Get().GetDatabase().DeleteImagesFromCollection(*collection, images);

        DualView::Get().InvokeFunction([=]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            GUARD_LOCK();

            ReloadImages(guard);
        });
    });
}

void SingleCollection::_OnOpenSelectedInImporter()
{
    DualView::Get().OpenImporter(GetSelected());
}
// ------------------------------------ //
void SingleCollection::_UpdateDeletedStatus()
{
    if(!ShownCollection) {
        DeleteThisCollection->set_sensitive(false);
        return;
    }

    // Can't delete the uncategorized collection
    if(ShownCollection->GetID() == DATABASE_UNCATEGORIZED_COLLECTION_ID) {
        DeleteThisCollection->set_sensitive(false);
        return;
    }

    DeleteThisCollection->set_sensitive(true);

    if(ShownCollection->IsDeleted()) {
        DeleteThisCollection->set_label("Restore This Collection");
    } else {
        DeleteThisCollection->set_label("Delete This Collection");
    }
}

void SingleCollection::_OnDeleteRestorePressed()
{
    if(!ShownCollection || !ShownCollection->IsInDatabase())
        return;

    if(!ShownCollection->IsDeleted()) {
        auto isalive = GetAliveMarker();
        const auto collection = ShownCollection;

        if(!collection)
            return;

        DualView::Get().QueueDBThreadFunction([=]() {
            // Find images to be orphaned

            const auto wouldOrphan =
                DualView::Get()
                    .GetDatabase()
                    .SelectImagesThatWouldBecomeOrphanedWhenRemovedFromCollectionAG(
                        *collection);
            const auto orphanCount = wouldOrphan.size();

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);
                _PerformDelete(orphanCount);
            });
        });

        return;
    }

    try {
        auto action =
            DualView::Get().GetDatabase().SelectCollectionDeleteAction(*ShownCollection, true);

        if(!action)
            throw Exception("No action to undo found. Check the Undo window to see what "
                            "has deleted this collection.");

        if(!action->Undo())
            throw Exception("Action undo failed");

    } catch(const Exception& e) {

        auto* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());

        if(!parent)
            return;

        auto dialog = Gtk::MessageDialog(*parent, "Deleting / restoring the collection failed",
            false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE, true);

        dialog.set_secondary_text("Error: " + std::string(e.what()));
        dialog.run();
    }
}

void SingleCollection::_PerformDelete(size_t orphanCount)
{
    if(orphanCount > 0) {
        auto dialog = Gtk::MessageDialog(*this, "Delete Collection?", false,
            Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text(
            "This collection has " + std::to_string(orphanCount) +
            " image(s) that will be added to Uncategorized if this is deleted. Note that due "
            "to current technical design the images only get added to Uncategorized when it "
            "is no longer possible to undo this action, meaning that the images might be "
            "hidden for some time. Continue with delete?");
        int result = dialog.run();

        if(result != Gtk::RESPONSE_YES)
            return;
    }

    DualView::Get().GetDatabase().DeleteCollection(*ShownCollection);
}
