#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "core/Plugin.h"

#include "core/components/EasyEntryCompletion.h"

#include "leviathan/Common/BaseNotifiable.h"

#include <gtkmm.h>

#include <atomic>

namespace DV{

class SuperViewer;
class TagEditor;
class FolderSelector;
class TagCollection;
class SuperContainer;

class DownloadSetup;
class PageScanJob;

class ListItem;
class InternetImage;

struct SetupScanQueueData;

void QueueNextThing(std::shared_ptr<SetupScanQueueData> data, DownloadSetup* setup,
    IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned);

//! \brief Manages setting up a new gallery to be downloaded
//! \todo Merge single image selection and tag editing from Importer to a base class
class DownloadSetup : public BaseWindow, public Gtk::Window, public IsAlive{

    friend void QueueNextThing(std::shared_ptr<SetupScanQueueData> data, DownloadSetup* setup,
        IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned);

    enum class STATE{

        //! Url has changed and is waiting to be accepted
        URL_CHANGED,
        
        CHECKING_URL,

        //! Main state that is active when everything is good
        URL_OK,

        //! Set when going through all the pages
        SCANNING_PAGES,
        
        //! Set when OK has been pressed
        ADDING_TO_DB
    };
    
public:

    DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~DownloadSetup();

    //! \brief Accepts this window settings and closes
    void OnUserAcceptSettings();
    

    //! \brief Called when the url is changed and it should be scanned again
    void OnURLChanged();

    //! \brief When the user edits the current url it should invalidate stuff
    void OnInvalidateURL();

    //! \brief Adds a page to scan when looking for images
    void AddSubpage(const std::string &url);

    //! \brief Starts page scanning if not currently running
    void StartPageScanning();

    //! \brief Adds an image to the list of found images
    void OnFoundContent(const ScanFoundImage &content);

    //! \brief Returns true if a new image link can be added
    bool IsValidTargetForImageAdd() const;

    //! \brief Returns true if this has no url and no collection name
    bool IsValidForNewPageScan() const;

    //! \brief Returns true if a valid target for adding content links
    //!
    //! AddExternalScanLink
    bool IsValidTargetForScanLink() const;

    //! \brief Sets this invalid for both IsValidTargetForImageAdd and IsValidForNewPageScan
    void SetLockActive();
    
    //! \brief Adds an external link to this window
    void AddExternallyFoundLink(const std::string &url, const std::string &referrer);

    //! \brief Sets the url
    void SetNewUrlToDl(const std::string &url);
    

    void AddExternalScanLink(const std::string &url);
    
    void SetTargetCollectionName(const std::string &str);

    //! \returns True if ready to download
    bool IsReadyToDownload() const;

    //! \brief Selects all found images
    void SelectAllImages();

    //! \brief Deselects all found images
    void DeselectAllImages();

    //! \brief Moves to next image
    void SelectNextImage();

    //! \brief Moves to previous image
    void SelectPreviousImage();
    
protected:

    void _OnClose() override;

    //! \brief Called after the url check has finished
    void UrlCheckFinished(bool wasvalid, const std::string &message);

    //! \brief Updates State and runs the update widget states on the main thread
    void _SetState(STATE newstate);

    void _UpdateWidgetStates();


    void OnItemSelected(ListItem &item);

    std::vector<std::shared_ptr<InternetImage>> GetSelectedImages();

    void UpdateReadyStatus();

    //! Updates the images whose tags are edited
    void UpdateEditedImages();

    void _OnFinishAccept(bool success);
    
private:

    //! Main state, controls what buttons can be pressed
    //! \todo When changed queue a "set sensitive" task on the main thread
    std::atomic<STATE> State = { STATE::URL_CHANGED };

    //! Found list of pages
    std::vector<std::string> PagesToScan;

    //! Found list of images
    std::vector<ScanFoundImage> ImagesToDownload;
    //! Actual list of InternetImages that are added to the DownloadableCollection
    //! when done setting up this download
    std::vector<std::shared_ptr<InternetImage>> ImageObjects;

    //! If true OnURLChanged callback is running.
    //! This is used to avoid stackoverflows when rewriting URLs
    bool UrlBeingChecked = false;

    //! Holds the original url that is being checked. Can be used to
    //! get the original URL when URL rewriting has changed it
    std::string CurrentlyCheckedURL;
    

    Gtk::Button* OKButton;
    Gtk::Label* MainStatusLabel;


     
    FolderSelector* TargetFolder;
    
    TagEditor* CollectionTagEditor;
    std::shared_ptr<TagCollection> CollectionTags;

    TagEditor* CurrentImageEditor;
    SuperViewer* CurrentImage;

    // Url entry
    Gtk::Entry* URLEntry;
    Gtk::Label* DetectedSettings;
    Gtk::Spinner* URLCheckSpinner;


    // Scanning
    Gtk::Label* PageRangeLabel;
    Gtk::Button* ScanPages;
    Gtk::Spinner* PageScanSpinner;
    Gtk::LinkButton* CurrentScanURL;
    Gtk::LevelBar* PageScanProgress;


    Gtk::Entry* TargetCollectionName;
    EasyEntryCompletion CollectionNameCompletion;

    // Tag editing / Image list view
    SuperContainer* ImageSelection;
    Gtk::CheckButton* SelectOnlyOneImage;
    Gtk::Button* DeselectImages;
    Gtk::Button* ImageSelectPageAll;

    Gtk::CheckButton* RemoveAfterAdding;
    Gtk::Switch* LockFromAdding;
    

    Gtk::Button* BrowseForward;
    Gtk::Button* BrowseBack;
        

    Gtk::Button* SelectAllImagesButton;
    
        
};

}

