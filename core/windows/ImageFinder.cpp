// ------------------------------------ //
#include "ImageFinder.h"

#include "DualView.h"
#include "Database.h"

#include "core/resources/Tags.h"
#include "core/components/SuperContainer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
ImageFinder::ImageFinder(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    builder->get_widget_derived("FoundImageContainer", Container);
    LEVIATHAN_ASSERT(Container, "Invalid .glade file");

    BUILDER_GET_WIDGET(MainSearchBar);
    MainSearchBar->signal_changed().connect(sigc::mem_fun(*this,
            &ImageFinder::OnSearchChanged));

    BUILDER_GET_WIDGET(SearchActiveSpinner);

    BUILDER_GET_WIDGET(FoundImageCountLabel);

    BUILDER_GET_WIDGET(SelectStatusLabel);
    
}

ImageFinder::~ImageFinder(){

    Close();
}

void ImageFinder::_OnClose(){

    // Close db queries if possible
}
// ------------------------------------ //
void ImageFinder::OnSearchChanged(){

    DualView::IsOnMainThreadAssert();

    auto isalive = GetAliveMarker();
    std::string matchingPattern = MainSearchBar->get_text();

    if(matchingPattern.empty())
        return;
    
    _SetSearchingState(true);

    DualView::Get().QueueDBThreadFunction([=](){

            // Parse tag and then do stuff //
            // TODO: separators
            std::shared_ptr<AppliedTag> tags;
            try{
                tags = DualView::Get().ParseTagFromString(matchingPattern);

                if(!tags)
                    throw Leviathan::InvalidArgument("empty");
                
            } catch(const Leviathan::InvalidArgument &e){

                INVOKE_CHECK_ALIVE_MARKER(isalive);
                
                _OnFailSearch(std::string("Invalid tag: ") +
                    e.what());
                
                return;
            }

            const auto dbTag = DualView::Get().GetDatabase().
                SelectExistingAppliedTagIDAG(*tags);

            if(dbTag == -1){

                INVOKE_CHECK_ALIVE_MARKER(isalive);
                
                _OnFailSearch(std::string("No resource has tag: ") + tags->ToAccurateString());
                return;
            }

            const auto images = DualView::Get().GetDatabase().SelectImageByTagAG(dbTag);

            auto loadedResources =
                std::make_shared<std::vector<std::shared_ptr<ResourceWithPreview>>>();

            loadedResources->insert(std::end(*loadedResources), std::begin(images),
                std::end(images));
            
            auto itemselect = std::make_shared<ItemSelectable>([=](ListItem &updateditem){

                    DualView::Get().InvokeFunction([=](){

                            INVOKE_CHECK_ALIVE_MARKER(isalive);

                            const auto count = Container->CountSelectedItems();
                            if(count == 0){

                                SelectStatusLabel->set_text("Nothing selected");
                            } else {

                                SelectStatusLabel->set_text("Selected " +
                                    std::to_string(count)+ " items");
                            }
                        });
                });

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    Container->SetShownItems(loadedResources->begin(), loadedResources->end(),
                        itemselect);

                    _SetSearchingState(false);

                    // Update status label about how many things we have found //
                    FoundImageCountLabel->set_text("Found " +
                        std::to_string(loadedResources->size()) + " images");
                });
        });
}
// ------------------------------------ //
void ImageFinder::_SetSearchingState(bool active){

    SearchActiveSpinner->property_active() = active;

    if(active)
        FoundImageCountLabel->set_text("Searching ..."); 
}

void ImageFinder::_OnFailSearch(const std::string &message){

    auto isalive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=](){

            INVOKE_CHECK_ALIVE_MARKER(isalive);

            _SetSearchingState(false);

            FoundImageCountLabel->set_text(message); 
        });
}

