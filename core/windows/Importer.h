#pragma once

#include "BaseWindow.h"

#include <gtkmm.h>


namespace DV{

class SuperContainer;
class SuperViewer;
class Image;
class ListItem;
class TagEditor;
class ResourceWithPreview;

class Importer : public BaseWindow, public Gtk::Window{
public:

    Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Importer();

    //! \brief Adds content from a file or a folder
    //!
    //! If the path refers to a folder no subdirectories are searched, unless recursive is true
    void FindContent(const std::string &path, bool recursive = false);

    //! \brief Updates the status label based on selected images
    void UpdateReadyStatus();
    
protected:

    //! Adds an image to the list
    //! \return True if the file extension is a valid image, false if not
    bool _AddImageToList(const std::string &file);
    
    bool _OnClosed(GdkEventAny* event);

    void _OnClose() override;

    //! File drag received
    void _OnFileDropped(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        const Gtk::SelectionData& selection_data, guint info, guint time);

    bool _OnDragMotion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        guint time);

    bool _OnDrop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);

    // Button callbacks
    void _OnDeselectAll();
    void _OnSelectAll();
    
    void OnItemSelected(ListItem &item);

protected:

    SuperViewer* PreviewImage;
    SuperContainer* ImageList;

    TagEditor* SelectedImageTags;

    Gtk::Label* StatusLabel;
    Gtk::CheckButton* SelectOnlyOneImage;

    bool DoingImport = false;

    //! List of images that might be marked as selected
    std::vector<std::shared_ptr<Image>> ImagesToImport;

    //! List of images that are selected currently, updated in UpdateReadyStatus
    std::vector<std::shared_ptr<Image>> SelectedImages;

    //! Keeps selected image memory loaded
    std::vector<std::shared_ptr<ResourceWithPreview>> SelectedItems;
};
      

}
