// ------------------------------------ //
#include "FolderSelector.h"

#include "core/resources/Folder.h"
#include "FolderListItem.h"

#include "DualView.h"
#include "Database.h"

using namespace DV;
// ------------------------------------ //


FolderSelector::FolderSelector() :
    CreateNewFolder(Gtk::StockID("gtk-new")),
    UpFolder(Gtk::StockID("gtk-go-up"))
{

    _CommonCtor();
}
    
FolderSelector::FolderSelector(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Box(widget),
    CreateNewFolder(Gtk::StockID("gtk-new")),
    UpFolder(Gtk::StockID("gtk-go-up"))
{
    _CommonCtor();
}

void FolderSelector::_CommonCtor(){

    CreateNewFolder.set_always_show_image();
    UpFolder.set_always_show_image();
    UpFolder.set_margin_right(15);

    TopBox.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    TopBox.pack_start(PathEntry, true, true);
    TopBox.pack_end(CreateNewFolder, false, false);
    TopBox.pack_end(UpFolder, false, false);

    add(TopBox);
    //child_property_expand(TopBox) = true;

    add(FolderContents);
    child_property_expand(FolderContents) = true;

    set_hexpand(true);
    set_vexpand(true);

    // Attach events //
    UpFolder.signal_clicked().connect(sigc::mem_fun(*this, &FolderSelector::_OnUpFolder));
    CreateNewFolder.signal_clicked().connect(sigc::mem_fun(*this,
            &FolderSelector::_CreateNewFolder));

    PathEntry.signal_activate().connect(sigc::mem_fun(*this,
            &FolderSelector::_OnPathEntered));
    
    show_all_children();
    
    // Show the root folder
    GoToRoot();
}

FolderSelector::~FolderSelector(){
    
}
// ------------------------------------ //
void FolderSelector::GoToRoot(){

    GoToPath("Root/");
}

void FolderSelector::GoToPath(const std::string &path){

    SelectedFolder = DualView::Get().GetFolderFromPath(path);
    CurrentPath = path;

    if(!SelectedFolder)
        GoToRoot();

    OnFolderChanged();
}
void FolderSelector::MoveToSubfolder(const std::string &subfoldername){

    if(subfoldername.empty())
        return;

    if(CurrentPath.back() != '/' && subfoldername.front() != '/')
        CurrentPath += '/';

    CurrentPath += subfoldername;
    GoToPath(CurrentPath);
}
// ------------------------------------ //
void FolderSelector::OnFolderChanged(){

    LEVIATHAN_ASSERT(SelectedFolder, "SelectedFolder is null in FolderSelector");
    
    std::vector<std::shared_ptr<Folder>> folders =
        DualView::Get().GetDatabase().SelectFoldersInFolder(*SelectedFolder);

    auto changefolder = std::make_shared<ItemSelectable>();

    changefolder->AddFolderSelect([this](ListItem &item){

            FolderListItem* asfolder = dynamic_cast<FolderListItem*>(&item);

            if(!asfolder)
                return;

            MoveToSubfolder(asfolder->GetFolder()->GetName());
        });
    
    FolderContents.SetShownItems(folders.begin(), folders.end(), changefolder);

    PathEntry.set_text(CurrentPath);
}


// ------------------------------------ //
void FolderSelector::_OnUpFolder(){

    if(CurrentPath == "Root/")
        return;

    if(CurrentPath.size() < 2){

        GoToRoot();
        return;
    }

    // Cut of the last part of the path //
    size_t cutend = 0;

    // Scan backwards until a /. We start from the second to last character
    for(size_t i = CurrentPath.size() - 1 - 1; i > 0; --i){

        if(CurrentPath[i] == '/'){

            cutend = i + 1;
            break;
        }
    }

    if(cutend < 1 || cutend > CurrentPath.size()){

        // Cutting failed //
        LOG_ERROR("FolderSelector: path cutting failed. Path is: " + CurrentPath);
        GoToRoot();
        return;
    }
    
    GoToPath(CurrentPath.substr(0, cutend));
}

void FolderSelector::_CreateNewFolder(){

    // If the user has typed something in the entry use that as the name for the new folder //
    std::string userinput = PathEntry.get_text();

    userinput.erase(0, CurrentPath.length());
    // userinput is most likely empty here //

    
    Gtk::Window* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());
    LEVIATHAN_ASSERT(parent, "FolderSelector has no Parent Gtk::Window");
    
    DualView::Get().RunFolderCreatorAsDialog(CurrentPath, userinput, *parent);

    // Update folders
    OnFolderChanged();
}
// ------------------------------------ //
void FolderSelector::_OnPathEntered(){

    GoToPath(PathEntry.get_text());
}
