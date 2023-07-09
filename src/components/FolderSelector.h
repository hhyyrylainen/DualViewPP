#pragma once

#include <memory>

#include <gtkmm.h>

#include "FolderNavigatorHelper.h"
#include "SuperContainer.h"

namespace DV
{

class Folder;

//! \brief Allows selecting a DV::Folder
class FolderSelector : public Gtk::Box,
                       public FolderNavigatorHelper
{
public:
    //! \brief Non-glade constructor
    FolderSelector();

    //! \brief Constructor called by glade builder when loading a widget of this type
    FolderSelector(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~FolderSelector() override = default;

    inline bool TargetPathLockedIn() const
    {
        return ControllingGlobalTarget;
    }

    //! \brief Returns the current folder. This is never null
    std::shared_ptr<Folder> GetFolder() const
    {
        return CurrentFolder;
    }

    //! \brief Returns the current path string
    VirtualPath GetPath() const
    {
        return VirtualPath(PathEntry.get_text());
    }

private:
    void _CommonCtor();

    static bool HasLockedInGlobalTargetPath();
    static VirtualPath GetLockedInGlobalTargetPath();
    static void SetLockedInGlobalTargetPath(const VirtualPath& path);
    static void ClearLockedInGlobalTargetPath();

protected:
    //! \brief Reads the properties of the SelectedFolder
    void OnFolderChanged() override;

    // Gtk callbacks //
    void _CreateNewFolder();

    void OnLockTargetModeChanged();

protected:
    //! Shows the CurrentPath to the user and can be used to paste in a path
    Gtk::Entry PathEntry;

    Gtk::Button CreateNewFolder;
    Gtk::Button UpFolder;

    Gtk::Box TopBox;

    //! Contents of the current folder
    SuperContainer FolderContents;

    Gtk::CheckButton TargetLocationLocked;

    bool ControllingGlobalTarget = false;

private:
    static bool RememberingGlobalPath;
    static VirtualPath RememberedGlobalPath;
};

} // namespace DV
