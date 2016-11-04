// ------------------------------------ //
#include "FolderNavigatorHelper.h"

#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //

void FolderNavigatorHelper::GoToRoot(){

    GoToPath("Root/");
}

void FolderNavigatorHelper::GoToPath(const std::string &path){

    CurrentFolder = DualView::Get().GetFolderFromPath(path);
    CurrentPath = path;

    if(!CurrentFolder)
        GoToRoot();

    OnFolderChanged();
}
// ------------------------------------ //
bool FolderNavigatorHelper::TryGoToPath(const std::string &path){

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

    if(CurrentPath.back() != '/' && subfoldername.front() != '/')
        CurrentPath += '/';

    CurrentPath += subfoldername;
    GoToPath(CurrentPath);
}

// ------------------------------------ //
void FolderNavigatorHelper::_OnUpFolder(){

    if(CurrentPath == "Root/" || CurrentPath == "Root")
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

            cutend = i;
            break;
        }
    }

    if(cutend < 1 || cutend > CurrentPath.size()){

        // Cutting failed //
        LOG_ERROR("FolderNavigator: path cutting failed. cut: " + Convert::ToString(cutend) +
            " Path is: " + CurrentPath);
        GoToRoot();
        return;
    }
    
    GoToPath(CurrentPath.substr(0, cutend + 1));
}

void FolderNavigatorHelper::_OnPathEntered(){

    if(!NavigatorPathEntry)
        return;

    if(!TryGoToPath(NavigatorPathEntry->get_text())){

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
