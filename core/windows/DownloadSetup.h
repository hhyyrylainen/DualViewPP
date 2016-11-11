#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "leviathan/Common/BaseNotifiable.h"

#include <gtkmm.h>

namespace DV{

class SuperViewer;
class TagEditor;
class FolderSelector;
class TagCollection;
class SuperContainer;

//! \brief Manages setting up a new gallery to be downloaded
class DownloadSetup : public BaseWindow, public Gtk::Window, public IsAlive{
public:

    DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~DownloadSetup();
    
protected:

    void _OnClose() override;
    
private:

    Gtk::Button* OKButton;

    SuperContainer* ImageSelection;
     
    FolderSelector* TargetFolder;
    
    TagEditor* CollectionTagEditor;
    std::shared_ptr<TagCollection> CollectionTags;

    TagEditor* CurrentImageEditor;
    SuperViewer* CurrentImage;
        
};

}

