#pragma once

#include "BaseWindow.h"

#include "core/components/EasyEntryCompletion.h"

#include "core/IsAlive.h"

#include <gtkmm.h>

#include <thread>
#include <atomic>


namespace DV{

class SuperContainer;
class SuperViewer;
class Image;
class ListItem;
class ResourceWithPreview;
class TagCollection;

class TagEditor;
class FolderSelector;

class Importer : public BaseWindow, public Gtk::Window, public IsAlive{
public:

    Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Importer();

    //! \brief Adds content from a file or a folder
    //!
    //! If the path refers to a folder no subdirectories are searched, unless recursive is true
    void FindContent(const std::string &path, bool recursive = false);

    //! \brief Adds existing database images to this Importer
    void AddExisting(const std::vector<std::shared_ptr<Image>> &images);

    //! \brief Updates the status label based on selected images
    void UpdateReadyStatus();

    //! \brief Starts importing the selected images
    //! \returns True if import started, false if another import is already in progress
    bool StartImporting(bool move);

protected:

    //! Ran in the importer thread
    void _RunImportThread(const std::string &collection, bool move);

    //! Ran in the main thread after importing finishes
    void _OnImportFinished(bool success);

    //! Adds an image to the list
    //! \return True if the file extension is a valid image, false if not
    bool _AddImageToList(const std::string &file);
    
    bool _OnClosed(GdkEventAny* event);

    void _OnClose() override;

    //! \brief Call when ImagesToImport is updated to update the list of items
    void _UpdateImageList();

    //! File drag received
    void _OnFileDropped(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        const Gtk::SelectionData& selection_data, guint info, guint time);

    bool _OnDragMotion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        guint time);

    bool _OnDrop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);

    // Button callbacks
    void _OnDeselectAll();
    void _OnSelectAll();
    void _OnCopyToCollection();
    void _OnMoveToCollection();
    void _OnBrowseForImages();
    void _OnAddImagesFromFolder();

    void _OnImportProgress();
    
    void OnItemSelected(ListItem &item);

protected:

    SuperViewer* PreviewImage;
    SuperContainer* ImageList;

    TagEditor* SelectedImageTags;
    TagEditor* CollectionTagsEditor;

    FolderSelector* TargetFolder;

    Gtk::Entry* CollectionName;
    EasyEntryCompletion CollectionNameCompletion;
    
    Gtk::Label* StatusLabel;
    Gtk::CheckButton* SelectOnlyOneImage;
    Gtk::CheckButton* RemoveAfterAdding;
    Gtk::CheckButton* DeleteImportFoldersIfEmpty;

    Gtk::LevelBar* ProgressBar;
    
    std::atomic<bool> DoingImport = { false };
    std::thread ImportThread;

    //! After importing these folders should be deleted if empty
    std::vector<std::string> FoldersToDelete;

    //! Tags to set on the target collection
    std::shared_ptr<TagCollection> CollectionTags;

    //! Import progress is reported through this
    Glib::Dispatcher ProgressDispatcher;
    std::atomic<float> ReportedProgress;

    //! List of images that might be marked as selected
    std::vector<std::shared_ptr<Image>> ImagesToImport;

    //! List of images that are selected currently, updated in UpdateReadyStatus
    std::vector<std::shared_ptr<Image>> SelectedImages;

    //! Keeps selected image memory loaded
    std::vector<std::shared_ptr<ResourceWithPreview>> SelectedItems;
};
      

}
