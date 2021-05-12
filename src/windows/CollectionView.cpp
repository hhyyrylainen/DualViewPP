// ------------------------------------ //
#include "CollectionView.h"

#include "Database.h"
#include "DualView.h"

#include "components/FolderListItem.h"
#include "components/SuperContainer.h"
#include "resources/Collection.h"
#include "resources/Folder.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
CollectionView::CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &CollectionView::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &CollectionView::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &CollectionView::_OnShown));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonLibrary", Menu, MenuPopover);

    builder->get_widget_derived("ImageContainer", Container);
    LEVIATHAN_ASSERT(Container, "Invalid .glade file");

    builder->get_widget("Path", PathEntry);
    LEVIATHAN_ASSERT(PathEntry, "Invalid .glade file");

    builder->get_widget("UpFolder", UpFolder);
    LEVIATHAN_ASSERT(UpFolder, "Invalid .glade file");

    RegisterNavigator(*PathEntry, *UpFolder);

    BUILDER_GET_WIDGET(SearchBox);
    SearchBox->signal_search_changed().connect(
        sigc::mem_fun(*this, &CollectionView::OnSearchChanged));
}

CollectionView::~CollectionView()
{
    LOG_INFO("CollectionView closed");
}
// ------------------------------------ //
bool CollectionView::_OnClose(GdkEventAny* event)
{
    // Just hide it //
    hide();

    return true;
}
// ------------------------------------ //
void CollectionView::_OnShown()
{
    GoToRoot();
}

void CollectionView::_OnHidden()
{
    // Explicitly unload items //
    auto empty = std::vector<std::shared_ptr<ResourceWithPreview>>();

    Container->SetShownItems(empty.begin(), empty.end());
}
// ------------------------------------ //
void CollectionView::OnSearchChanged()
{
    _UpdateShownItems();
}

void CollectionView::OnFolderChanged()
{
    if(!DualView::IsOnMainThread()) {

        // Invoke on the main thread //
        INVOKE_FUNCTION_WITH_ALIVE_CHECK(OnFolderChanged);
        return;
    }

    DualView::IsOnMainThreadAssert();

    FolderWasChanged = true;

    PathEntry->set_text(CurrentPath.GetPathString());

    // Load items //
    if(SearchBox->get_text().empty()) {

        _UpdateShownItems();

    } else {

        // This calls _UpdateShownItems from the change callback (OnSearchChanged)
        SearchBox->set_text("");
    }
}

void CollectionView::_UpdateShownItems()
{
    auto isalive = GetAliveMarker();
    std::string matchingpattern = SearchBox->get_text();

    auto folder = CurrentFolder;

    if(!folder) {

        Container->Clear();
        return;
    }

    bool folderChanged = FolderWasChanged;
    FolderWasChanged = false;

    DualView::Get().QueueDBThreadFunction([=]() {
        const auto collections =
            DualView::Get().GetDatabase().SelectCollectionsInFolderAG(*folder, matchingpattern);

        const auto folders =
            DualView::Get().GetDatabase().SelectFoldersInFolderAG(*folder, matchingpattern);

        auto loadedresources =
            std::make_shared<std::vector<std::shared_ptr<ResourceWithPreview>>>();

        loadedresources->insert(
            std::end(*loadedresources), std::begin(folders), std::end(folders));

        loadedresources->insert(
            std::end(*loadedresources), std::begin(collections), std::end(collections));

        auto changefolder = std::make_shared<ItemSelectable>();

        changefolder->AddFolderSelect([this](ListItem& item) {
            auto* asfolder = dynamic_cast<FolderListItem*>(&item);

            if(!asfolder)
                return;

            // This callback isn't recreated when changing folders so we need to use
            // CurrentPath here
            GoToPath(CurrentPath / VirtualPath(asfolder->GetFolder()->GetName()));
        });

        DualView::Get().InvokeFunction(
            [this, isalive, loadedresources, changefolder, folderChanged]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);

                Container->SetShownItems(loadedresources->begin(), loadedresources->end(),
                    changefolder,
                    folderChanged ? SuperContainer::POSITION_KEEP_MODE::SCROLL_TO_TOP :
                                    SuperContainer::POSITION_KEEP_MODE::SCROLL_TO_EXISTING);
            });
    });
}
