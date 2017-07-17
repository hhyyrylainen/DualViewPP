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
    
    _SetSearchingState(true);

    auto isalive = GetAliveMarker();
    std::string matchingPattern = MainSearchBar->get_text();

    if(matchingPattern.empty())
        return;

    DualView::Get().QueueDBThreadFunction([=](){

            // Parse tag and then do stuff //
            // TODO: separators
            std::shared_ptr<AppliedTag> tags;
            try{
                tags = DualView::Get().ParseTagFromString(matchingPattern);
                LEVIATHAN_ASSERT(tags, "got null tags and no exception");
                
            } catch(const Leviathan::InvalidArgument &e){

                INVOKE_CHECK_ALIVE_MARKER(isalive);
                
                _OnFailSearch(std::string("Invalid tag: ") +
                    e.what());
                
                return;
            }

            const auto dbTag = DualView::Get().GetDatabase().SelectExistingAppliedTagAG(*tags);

            if(!dbTag){

                INVOKE_CHECK_ALIVE_MARKER(isalive);
                
                _OnFailSearch(std::string("No resource has tag: ") + tags->ToAccurateString());
                return;
            }

            const auto images = DualView::Get().GetDatabase().SelectImageByTagAG(*dbTag);

            auto loadedresources =
                std::make_shared<std::vector<std::shared_ptr<ResourceWithPreview>>>();

            loadedresources->insert(std::end(*loadedresources), std::begin(images),
                std::end(images));
            
            auto itemselect = std::make_shared<ItemSelectable>();

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    Container->SetShownItems(loadedresources->begin(), loadedresources->end(),
                        itemselect);

                    _SetSearchingState(false);

                    // Update status label about how many things we have found //
                    
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

