#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "leviathan/Common/BaseNotifiable.h"

#include <gtkmm.h>

#include <atomic>

namespace DV{

class SuperViewer;
class TagEditor;
class FolderSelector;
class TagCollection;
class SuperContainer;

//! \brief Manages setting up a new gallery to be downloaded
class DownloadSetup : public BaseWindow, public Gtk::Window, public IsAlive{

    enum class STATE{

        //! Url has changed and is waiting to be accepted
        URL_CHANGED,
        
        CHECKING_URL,

        URL_OK
    };
    
public:

    DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~DownloadSetup();

    //! \brief Called when the url is changed and it should be scanned again
    void OnURLChanged();

    //! \brief When the user edits the current url it should invalidate stuff
    void OnInvalidateURL();

    //! \brief Adds a page to scan when looking for images
    void AddSubpage(const std::string &url);
    
protected:

    void _OnClose() override;

    //! \brief Called after the url check has finished
    void UrlCheckFinished(bool wasvalid, const std::string &message);

    //! \brief Updates State and runs the update widget states on the main thread
    void _SetState(STATE newstate);

    void _UpdateWidgetStates();
    
private:

    //! Main state, controls what buttons can be pressed
    //! \todo When changed queue a "set sensitive" task on the main thread
    std::atomic<STATE> State = { STATE::URL_CHANGED };

    //! Found list of pages
    std::vector<std::string> PagesToScan;

    //! If true OnURLChanged callback is running.
    //! This is used to avoid stackoverflows when rewriting URLs
    bool UrlBeingChecked = false;

    //! Holds the original url that is being checked. Can be used to
    //! get the original URL when URL rewriting has changed it
    std::string CurrentlyCheckedURL;
    

    Gtk::Button* OKButton;

    SuperContainer* ImageSelection;
     
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



    
        
};

}

