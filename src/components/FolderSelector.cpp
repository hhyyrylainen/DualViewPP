// ------------------------------------ //
#include "FolderSelector.h"

#include "FolderListItem.h"
#include "resources/Folder.h"

#include "Database.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //


FolderSelector::FolderSelector() : Gtk::Box(Gtk::ORIENTATION_VERTICAL)
{
    _CommonCtor();
}

FolderSelector::FolderSelector(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Box(widget)
{
    _CommonCtor();
}

void FolderSelector::_CommonCtor()
{
    CreateNewFolder.set_image_from_icon_name("folder-new-symbolic");
    UpFolder.set_image_from_icon_name("go-up-symbolic");

    FolderContents.SetItemSize(LIST_ITEM_SIZE::SMALL);

    CreateNewFolder.set_always_show_image();
    UpFolder.set_always_show_image();
    UpFolder.set_margin_end(15);

    TopBox.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    TopBox.pack_start(PathEntry, true, true);
    TopBox.pack_end(CreateNewFolder, false, false);
    TopBox.pack_end(UpFolder, false, false);

    add(TopBox);
    // child_property_expand(TopBox) = true;

    add(FolderContents);
    child_property_expand(FolderContents) = true;

    set_hexpand(true);
    set_vexpand(true);

    // Attach events //
    CreateNewFolder.signal_clicked().connect(
        sigc::mem_fun(*this, &FolderSelector::_CreateNewFolder));

    show_all_children();

    RegisterNavigator(PathEntry, UpFolder);

    // Show the root folder
    GoToRoot();
}

FolderSelector::~FolderSelector() {}
// ------------------------------------ //
void FolderSelector::OnFolderChanged()
{
    if(!CurrentFolder) {

        LOG_ERROR("SelectedFolder is null in FolderSelector");
        return;
    }

    // TODO: move this load to a background thread
    std::vector<std::shared_ptr<Folder>> folders =
        DualView::Get().GetDatabase().SelectFoldersInFolderAG(*CurrentFolder);

    auto changefolder = std::make_shared<ItemSelectable>();

    changefolder->AddFolderSelect([this](ListItem& item) {
        FolderListItem* asfolder = dynamic_cast<FolderListItem*>(&item);

        if(!asfolder)
            return;

        MoveToSubfolder(asfolder->GetFolder()->GetName());
    });

    FolderContents.SetShownItems(folders.begin(), folders.end(), changefolder,
        SuperContainer::POSITION_KEEP_MODE::SCROLL_TO_TOP);

    PathEntry.set_text(CurrentPath.GetPathString());
}
// ------------------------------------ //
void FolderSelector::_CreateNewFolder()
{
    // If the user has typed something in the entry use that as the name for the new folder //
    std::string userinput = PathEntry.get_text();

    userinput.erase(0, CurrentPath.GetPathString().length());
    // userinput is most likely empty here //


    Gtk::Window* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());
    LEVIATHAN_ASSERT(parent, "FolderSelector has no Parent Gtk::Window");

    DualView::Get().RunFolderCreatorAsDialog(CurrentPath, userinput, *parent);

    // Update folders
    OnFolderChanged();
}
// ------------------------------------ //
