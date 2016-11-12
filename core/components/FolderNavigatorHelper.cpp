// ------------------------------------ //
#include "FolderNavigatorHelper.h"

#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //

void FolderNavigatorHelper::GoToRoot(){

    GoToPath(VirtualPath("Root/"));
}

void FolderNavigatorHelper::GoToPath(const VirtualPath &path){

    CurrentFolder = DualView::Get().GetFolderFromPath(path);
    CurrentPath = path;

    if(!CurrentFolder)
        GoToRoot();

    OnFolderChanged();
}
// ------------------------------------ //
bool FolderNavigatorHelper::TryGoToPath(const VirtualPath &path){

    auto folder = DualView::Get().GetFolderFromPath(path);

    if(!folder)
        return false;

    CurrentFolder = folder;
    CurrentPath = path;

    OnFolderChanged();
    return true;
}
// ------------------------------------ //
void FolderNavigatorHelper::MoveToSubfolder(const std::string &subfoldername){

    if(subfoldername.empty())
        return;

    CurrentPath = CurrentPath / VirtualPath(subfoldername);
    GoToPath(CurrentPath);
}
// ------------------------------------ //
void FolderNavigatorHelper::_OnUpFolder(){

    --CurrentPath;
    GoToPath(CurrentPath);
}

void FolderNavigatorHelper::_OnPathEntered(){

    if(!NavigatorPathEntry)
        return;

    if(!TryGoToPath(VirtualPath(NavigatorPathEntry->get_text(), false))){

        LOG_ERROR("FolderNavigator: TODO: error sound");
    }
}

void FolderNavigatorHelper::RegisterNavigator(Gtk::Entry &pathentry, Gtk::Button &upfolder){

    upfolder.signal_clicked().connect(sigc::mem_fun(*this,
            &FolderNavigatorHelper::_OnUpFolder));
    
    pathentry.signal_activate().connect(sigc::mem_fun(*this,
            &FolderNavigatorHelper::_OnPathEntered));

    NavigatorPathEntry = &pathentry;
}
