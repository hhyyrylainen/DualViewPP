// ------------------------------------ //
#include "CollectionView.h"

#include "DualView.h"
#include "Database.h"

#include "core/components/SuperContainer.h"
#include "core/components/FolderListItem.h"
#include "core/resources/Folder.h"
#include "core/resources/Collection.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
CollectionView::CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{

    signal_delete_event().connect(sigc::mem_fun(*this, &CollectionView::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &CollectionView::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &CollectionView::_OnShown));
    
    builder->get_widget_derived("ImageContainer", Container);
    LEVIATHAN_ASSERT(Container, "Invalid .glade file");

    builder->get_widget("Path", PathEntry);
    LEVIATHAN_ASSERT(PathEntry, "Invalid .glade file");

    builder->get_widget("UpFolder", UpFolder);
    LEVIATHAN_ASSERT(UpFolder, "Invalid .glade file");

    RegisterNavigator(*PathEntry, *UpFolder);

    BUILDER_GET_WIDGET(SearchBox);
    SearchBox->signal_search_changed().connect(sigc::mem_fun(*this,
            &CollectionView::OnSearchChanged));
    
}

CollectionView::~CollectionView(){

    LOG_INFO("CollectionView closed");
}
// ------------------------------------ //
bool CollectionView::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();

    LOG_INFO("Hiding CollectionView");
    
    return true;
}
// ------------------------------------ //
void CollectionView::_OnShown(){

 
    GoToRoot();
}

void CollectionView::_OnHidden(){

    // Explicitly unload items //
    
}
// ------------------------------------ //
void CollectionView::OnSearchChanged(){

    OnFolderChanged();
}
        
void CollectionView::OnFolderChanged(){

    if(!DualView::IsOnMainThread()){

        // Invoke on the main thread //
        INVOKE_FUNCTION_WITH_ALIVE_CHECK(OnFolderChanged);
        return;
    }

    DualView::IsOnMainThreadAssert();
    
    PathEntry->set_text(CurrentPath.GetPathString());

    // Load items //
    auto isalive = GetAliveMarker();
    std::string matchingpattern = SearchBox->get_text();

    auto folder = CurrentFolder;

    if(!folder){

        Container->Clear();
        return;
    }

    const VirtualPath startFolder = CurrentPath;

    DualView::Get().QueueDBThreadFunction([=](){

            const auto collections = DualView::Get().GetDatabase().SelectCollectionsInFolder(
                *folder, matchingpattern);

            const auto folders = DualView::Get().GetDatabase().SelectFoldersInFolder(
                *folder, matchingpattern);
            
            auto loadedresources =
                std::make_shared<std::vector<std::shared_ptr<ResourceWithPreview>>>();

            loadedresources->insert(std::end(*loadedresources), std::begin(folders),
                std::end(folders));
            
            loadedresources->insert(std::end(*loadedresources), std::begin(collections),
                std::end(collections));

            auto changefolder = std::make_shared<ItemSelectable>();

            changefolder->AddFolderSelect([this, startFolder](ListItem &item){

                    FolderListItem* asfolder = dynamic_cast<FolderListItem*>(&item);

                    if(!asfolder)
                        return;
                    
                    // This works even if the user clicks multiple folders quickly and
                    // appending the names would create an invalid path
                    GoToPath(startFolder /
                        VirtualPath(asfolder->GetFolder()->GetName()));
                });

            DualView::Get().InvokeFunction([this, isalive, loadedresources, changefolder](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    Container->SetShownItems(loadedresources->begin(), loadedresources->end(),
                        changefolder);
                });
        });
}

