// ------------------------------------ //
#include "SingleCollection.h"

#include "core/resources/Collection.h"

#include "core/components/TagEditor.h"
#include "core/components/SuperContainer.h"

#include "DualView.h"
#include "Common.h"

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

    OpenTagEdit->signal_clicked().connect(sigc::mem_fun(*this,
            &SingleCollection::ToggleTagEditor));

    BUILDER_GET_WIDGET(DeleteSelected);

    DeleteSelected->signal_clicked().connect(sigc::mem_fun(*this,
            &SingleCollection::_OnDeleteSelected));
    
    BUILDER_GET_WIDGET(OpenSelectedImporter);

    OpenSelectedImporter->signal_clicked().connect(sigc::mem_fun(*this,
            &SingleCollection::_OnOpenSelectedInImporter));
}

SingleCollection::~SingleCollection(){

    Close();

    LOG_INFO("SingleCollection window destructed");
}
// ------------------------------------ //    
void SingleCollection::ShowCollection(std::shared_ptr<Collection> collection){

    // Detach old collection, if there is one //
    GUARD_LOCK();
    
    ReleaseParentHooks(guard);
    ShownCollection = collection;

    ReloadImages(guard);
}
// ------------------------------------ //
void SingleCollection::OnNotified(Lock &ownlock, Leviathan::BaseNotifierAll* parent,
    Lock &parentlock)
{
    ReloadImages(ownlock);
}

void SingleCollection::ReloadImages(Lock &guard){

    // Start listening for changes in the collection //
    if(ShownCollection)
        if(!IsConnectedTo(ShownCollection.get(), guard))
            ConnectToNotifier(guard, ShownCollection.get());

    StatusLabel->set_text("Loading Collection...");
    set_title((ShownCollection ? ShownCollection->GetName() : std::string("None")) +
        " - Collection - DualView++");

    if(CollectionTags->get_visible()){

        // Update tags //
        CollectionTags->SetEditedTags({ ShownCollection ? ShownCollection->GetTags() :
                    nullptr });
    }

    std::shared_ptr<Collection> collection = ShownCollection;
    if(!collection)
        return;

    auto isalive = GetAliveMarker();
        
    DualView::Get().QueueDBThreadFunction([this, isalive, collection](){

            auto images = collection->GetImages();

            DualView::Get().InvokeFunction([this, isalive, images, collection](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    ImageContainer->SetShownItems(images.begin(), images.end(),
                        std::make_shared<ItemSelectable>([=](ListItem &item){

                                bool hasselected = ImageContainer->CountSelectedItems() > 0;

                                // Enable buttons //
                                DeleteSelected->set_sensitive(hasselected);
                                OpenSelectedImporter->set_sensitive(hasselected);
                            }));
                        
                    StatusLabel->set_text("Collection \"" + collection->GetName() +
                        "\" Has " + Convert::ToString(images.size()) + " Images");
                });
        });
}
// ------------------------------------ //
void SingleCollection::_OnClose(){


}
// ------------------------------------ //
void SingleCollection::ToggleTagEditor(){

    if(CollectionTags->get_visible()){

        CollectionTags->SetEditedTags({});
        CollectionTags->hide();
        
    } else {

        CollectionTags->show();
        CollectionTags->SetEditedTags({
                ShownCollection ? ShownCollection->GetTags() : nullptr });
    }
}

std::vector<std::shared_ptr<Image>> SingleCollection::GetSelected() const{

    std::vector<std::shared_ptr<ResourceWithPreview>> items;
    ImageContainer->GetSelectedItems(items);

    std::vector<std::shared_ptr<Image>> imgs;
    imgs.reserve(items.size());

    for(const auto& i : items){

        auto casted = std::dynamic_pointer_cast<Image>(i);

        if(casted)
            imgs.push_back(casted);
    }

    return imgs;
}

void SingleCollection::_OnDeleteSelected(){
    
    auto isalive = GetAliveMarker();
    const auto images = GetSelected();
    const auto collection = ShownCollection;

    if(!collection)
        return;
        
    DualView::Get().QueueDBThreadFunction([=](){

            for(const auto& image : images)
                DualView::Get().RemoveImageFromCollection(image, collection);

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    GUARD_LOCK();

                    ReloadImages(guard);
                });
        });
}

void SingleCollection::_OnOpenSelectedInImporter(){

    DualView::Get().OpenImporter(GetSelected());
}

