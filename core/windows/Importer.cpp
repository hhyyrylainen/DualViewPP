// ------------------------------------ //
#include "Importer.h"

#include "core/components/SuperViewer.h"
#include "core/components/SuperContainer.h"
#include "core/components/TagEditor.h"

#include "core/resources/Image.h"

#include "core/DualView.h"
#include "Common.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //

Importer::Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    builder->get_widget_derived("PreviewImage", PreviewImage, nullptr, false);
    LEVIATHAN_ASSERT(PreviewImage, "Invalid .glade file");

    builder->get_widget_derived("ImageList", ImageList);
    LEVIATHAN_ASSERT(ImageList, "Invalid .glade file");

    builder->get_widget_derived("SelectedImageTags", SelectedImageTags);
    LEVIATHAN_ASSERT(SelectedImageTags, "Invalid .glade file");


    builder->get_widget("StatusLabel", StatusLabel);
    LEVIATHAN_ASSERT(StatusLabel, "Invalid .glade file");

    builder->get_widget("SelectOnlyOneImage", SelectOnlyOneImage);
    LEVIATHAN_ASSERT(SelectOnlyOneImage, "Invalid .glade file");

    Gtk::Button* DeselectAll;
    builder->get_widget("DeselectAll", DeselectAll);
    LEVIATHAN_ASSERT(DeselectAll, "Invalid .glade file");

    DeselectAll->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnDeselectAll));

    Gtk::Button* SelectAll;
    builder->get_widget("SelectAll", SelectAll);
    LEVIATHAN_ASSERT(SelectAll, "Invalid .glade file");

    SelectAll->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnSelectAll));
    
    signal_delete_event().connect(sigc::mem_fun(*this, &Importer::_OnClosed));

    // Dropping files into the list //
    std::vector<Gtk::TargetEntry> listTargets;
    listTargets.push_back(Gtk::TargetEntry("text/uri-list"));
    ImageList->drag_dest_set(listTargets, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP,
        Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
    
    ImageList->signal_drag_data_received().connect(sigc::mem_fun(*this, &Importer::_OnFileDropped));
    ImageList->signal_drag_motion().connect(sigc::mem_fun(*this, &Importer::_OnDragMotion));
    ImageList->signal_drag_drop().connect(sigc::mem_fun(*this, &Importer::_OnDrop));

    
    builder->get_widget("CollectionName", CollectionName);
    LEVIATHAN_ASSERT(CollectionName, "Invalid .glade file");

    Gtk::Button* CopyToCollection;
    builder->get_widget("CopyToCollection", CopyToCollection);
    LEVIATHAN_ASSERT(CopyToCollection, "Invalid .glade file");

    CopyToCollection->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnCopyToCollection));
}

Importer::~Importer(){

    LOG_INFO("Importer properly closed");
    Close();
}
// ------------------------------------ //
void Importer::FindContent(const std::string &path, bool recursive /*= false*/){

    namespace bf = boost::filesystem;
    
    LOG_INFO("Importer finding content from: " + path);

    if(!bf::is_directory(path)){

        // A single file //
        _AddImageToList(path);
        return;
    }

    // Set the target collection //
    LOG_INFO("TODO: set the target collection");

    // Loop contents //
    if(recursive){

        for (bf::recursive_directory_iterator iter(path);
             iter != bf::recursive_directory_iterator(); ++iter)
        {
            if(bf::is_directory(iter->status()))
                continue;
        
            // Add image //
            _AddImageToList(iter->path().string());
        }

        return;
    }

    for (bf::directory_iterator iter(path); iter != bf::directory_iterator(); ++iter)
    {
        if(bf::is_directory(iter->status()))
            continue;
        
        // Add image //
        _AddImageToList(iter->path().string());
    }
}

bool Importer::_AddImageToList(const std::string &file){

    if(!DualView::IsFileContent(file))
        return false;

    std::shared_ptr<Image> img;
    
    try{

        img = Image::Create(file);

    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("Failed to add image to importer:");
        e.PrintToLog();
        return false;
    }

    //ImagesToImport.clear();
    ImagesToImport.push_back(img);
    ImageList->SetShownItems(ImagesToImport.begin(), ImagesToImport.end(),
        ItemSelectable(std::bind(&Importer::OnItemSelected, this, std::placeholders::_1)));
    
    LOG_INFO("Importer added new image: " + file);
    return true;
}
// ------------------------------------ //    
bool Importer::_OnClosed(GdkEventAny* event){

    _ReportClosed();
    return false;
}

void Importer::_OnClose(){

    if(DoingImport){

        // Ask user to interrupt importing //
        LOG_WARNING("Importer closing while doing import");
    }

    if(ImportThread.joinable())
        ImportThread.join();
    

    close();

    // Todo: release logic
}
// ------------------------------------ //
void Importer::UpdateReadyStatus(){

    if(DoingImport){

        StatusLabel->set_text("Import in progress...");
        // TODO: set things not sensitive, so they are read only
        set_sensitive(false);
        return;
    }

    if(!get_sensitive())
        set_sensitive(true);

    SelectedImages.clear();
    SelectedItems.clear();
    
    ImageList->GetSelectedItems(SelectedItems);

    for(const auto& preview : SelectedItems){

        auto asImage = std::dynamic_pointer_cast<Image>(preview);

        if(!asImage){

            LOG_WARNING("Importer: SuperContainer has non-image items in it");
            continue;
        }
        
        SelectedImages.push_back(asImage);
    }

    if(SelectedImages.empty()){

        StatusLabel->set_text("No images selected");

    } else {

        StatusLabel->set_text("Ready to import " + Convert::ToString(SelectedImages.size()) +
            " images");
    }

    // Tag editing //
    std::vector<std::shared_ptr<TagCollection>> tagstoedit;
    
    for(const auto& image : SelectedImages){

        tagstoedit.push_back(image->GetTags());
    }

    SelectedImageTags->SetEditedTags(tagstoedit);
}

void Importer::OnItemSelected(ListItem &item){

    // Deselect others if only one is wanted //
    if(SelectOnlyOneImage->get_active() && item.IsSelected()){

        // Deselect all others //
        ImageList->DeselectAllExcept(&item);
    }
    
    UpdateReadyStatus();
}
// ------------------------------------ //
bool Importer::StartImporting(){

    bool expected = false;
    if(!DoingImport.compare_exchange_weak(expected, true,
            std::memory_order_release, std::memory_order_relaxed))
    {
        return false;
    }

    // Value was changed to true //
    ImportThread = std::thread(&Importer::_RunImportThread, this);

    // Update selected //
    UpdateReadyStatus();
    // Because DoingImport is true the above function only sets this to be not-sensitive
    
    return true;
}

void Importer::_RunImportThread(){

    
}

void Importer::_OnCopyToCollection(){

    StartImporting();
}
// ------------------------------------ //
void Importer::_OnDeselectAll(){

    ImageList->DeselectAllItems();
}

void Importer::_OnSelectAll(){

    // If the "select only one" checkbox is checked this doesn't work properly
    if(SelectOnlyOneImage->get_active()){

        SelectOnlyOneImage->set_active(false);
        
        ImageList->SelectAllItems();

        SelectOnlyOneImage->set_active(true);
        
    } else {

        ImageList->SelectAllItems();
    }
}
// ------------------------------------ //
bool Importer::_OnDragMotion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
    guint time)
{
    if(DoingImport){

        context->drag_refuse(time);
        return false;
    }

    const auto suggestion = context->get_suggested_action();

    context->drag_status(suggestion == Gdk::ACTION_MOVE ? suggestion : Gdk::ACTION_COPY, time);
    return true;
}

bool Importer::_OnDrop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
    guint time)
{
    if(DoingImport){
        return false;
    }

    // _OnFileDropped gets called next
    return true;
}

void Importer::_OnFileDropped(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
    const Gtk::SelectionData& selection_data, guint info, guint time)
{
    if ((selection_data.get_length() >= 0) && (selection_data.get_format() == 8))
    {

        std::vector<Glib::ustring> uriList;
 
        uriList = selection_data.get_uris();

        for(const auto &uri : uriList){

            Glib::ustring path = Glib::filename_from_uri(uri);

            FindContent(path);
        }
 
        context->drag_finish(true, false, time);
        return;
    }
 
    context->drag_finish(false, false, time);
}
// ------------------------------------ //
