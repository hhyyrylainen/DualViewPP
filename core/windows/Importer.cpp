// ------------------------------------ //
#include "Importer.h"

#include "core/components/SuperViewer.h"
#include "core/components/SuperContainer.h"
#include "core/components/TagEditor.h"
#include "core/components/FolderSelector.h"


#include "core/resources/Image.h"
#include "core/resources/Tags.h"
#include "core/resources/Folder.h"

#include "DualView.h"
#include "Database.h"
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

    builder->get_widget_derived("CollectionTags", CollectionTagsEditor);
    LEVIATHAN_ASSERT(CollectionTagsEditor, "Invalid .glade file");

    builder->get_widget_derived("TargetFolder", TargetFolder);
    LEVIATHAN_ASSERT(TargetFolder, "Invalid .glade file");

    builder->get_widget("StatusLabel", StatusLabel);
    LEVIATHAN_ASSERT(StatusLabel, "Invalid .glade file");

    builder->get_widget("SelectOnlyOneImage", SelectOnlyOneImage);
    LEVIATHAN_ASSERT(SelectOnlyOneImage, "Invalid .glade file");

    builder->get_widget("DeleteImportFoldersIfEmpty", DeleteImportFoldersIfEmpty);
    LEVIATHAN_ASSERT(DeleteImportFoldersIfEmpty, "Invalid .glade file");

    builder->get_widget("RemoveAfterAdding", RemoveAfterAdding);
    LEVIATHAN_ASSERT(RemoveAfterAdding, "Invalid .glade file");

    builder->get_widget("ProgressBar", ProgressBar);
    LEVIATHAN_ASSERT(ProgressBar, "Invalid .glade file");

    Gtk::Button* DeselectAll;
    builder->get_widget("DeselectAll", DeselectAll);
    LEVIATHAN_ASSERT(DeselectAll, "Invalid .glade file");

    DeselectAll->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnDeselectAll));

    Gtk::Button* SelectAll;
    builder->get_widget("SelectAll", SelectAll);
    LEVIATHAN_ASSERT(SelectAll, "Invalid .glade file");

    SelectAll->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnSelectAll));
    
    Gtk::Button* BrowseForImages;
    builder->get_widget("BrowseForImages", BrowseForImages);
    LEVIATHAN_ASSERT(BrowseForImages, "Invalid .glade file");

    BrowseForImages->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnBrowseForImages));

    Gtk::Button* AddImagesFromFolder;
    builder->get_widget("AddImagesFromFolder", AddImagesFromFolder);
    LEVIATHAN_ASSERT(AddImagesFromFolder, "Invalid .glade file");

    AddImagesFromFolder->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnAddImagesFromFolder));
    
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

    Gtk::Button* MoveToCollection;
    builder->get_widget("MoveToCollection", MoveToCollection);
    LEVIATHAN_ASSERT(MoveToCollection, "Invalid .glade file");

    MoveToCollection->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnMoveToCollection));

    // Progress dispatcher
    ProgressDispatcher.connect(sigc::mem_fun(*this, &Importer::_OnImportProgress));

    // Create the collection tag holder
    CollectionTags = std::make_shared<TagCollection>();
    CollectionTagsEditor->SetEditedTags({ CollectionTags });
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

    ImagesToImport.push_back(img);
    _UpdateImageList();
    
    LOG_INFO("Importer added new image: " + file);
    return true;
}

void Importer::_UpdateImageList(){

    ImageList->SetShownItems(ImagesToImport.begin(), ImagesToImport.end(),
        ItemSelectable(std::bind(&Importer::OnItemSelected, this, std::placeholders::_1)));
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
        PreviewImage->RemoveImage();

    } else {

        StatusLabel->set_text("Ready to import " + Convert::ToString(SelectedImages.size()) +
            " images");

        PreviewImage->SetImage(SelectedImages.front());
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
bool Importer::StartImporting(bool move){

    bool expected = false;
    if(!DoingImport.compare_exchange_weak(expected, true,
            std::memory_order_release, std::memory_order_relaxed))
    {
        return false;
    }

    // Value was changed to true //

    // Set progress //
    ReportedProgress = 0.01f;
    _OnImportProgress();
    
    // Run import in a new thread //
    ImportThread = std::thread(&Importer::_RunImportThread, this, CollectionName->get_text(),
        move);

    // Update selected //
    UpdateReadyStatus();
    // Because DoingImport is true the above function only sets this to be not-sensitive
    
    return true;
}

void Importer::_RunImportThread(const std::string &collection, bool move){

    bool result = DualView::Get().AddToCollection(SelectedImages, move, collection,
        *CollectionTags, [this](float progress){

            ReportedProgress = progress;
            ProgressDispatcher.emit();
        });

    // Invoke onfinish on the main thread //
    auto isalive = GetAliveMarker();
    
    DualView::Get().InvokeFunction([this, result, isalive](){

            INVOKE_CHECK_ALIVE_MARKER(isalive);
            
            _OnImportFinished(result);
        });
}

void Importer::_OnImportFinished(bool success){

    LEVIATHAN_ASSERT(DualView::IsOnMainThread(),
        "_OnImportFinished called on the wrong thread");

    ReportedProgress = 1.f;
    _OnImportProgress();

    // Remove images if succeeded //
    if(success){

        // Add the collection to the target folder //
        auto targetfolder = TargetFolder->GetFolder();

        if(!targetfolder->IsRoot()){

            DualView::Get().AddCollectionToFolder(targetfolder,
                DualView::Get().GetDatabase().SelectCollectionByNameAG(
                    CollectionName->get_text()));
        }

        LOG_INFO("Import was successfull");

        if(RemoveAfterAdding->get_active()){

            // Remove SelectedImages from ImagesToImport
            ImagesToImport.erase(std::remove_if(ImagesToImport.begin(), ImagesToImport.end(),
                    [this](auto &x)
                    {
                        return std::find(SelectedImages.begin(), SelectedImages.end(), x) !=
                            SelectedImages.end();
                    }),
                ImagesToImport.end());
            
            _UpdateImageList();
        }

        // Reset collection tags //
        CollectionTags->Clear();
        CollectionTagsEditor->ReadSetTags();

        CollectionName->set_text("");

        // Reset target folder //
        TargetFolder->GoToRoot();
        
    } else {

        LOG_INFO("TODO: popup error box");
    }
    
    // Reset variables //
    SelectedImages.clear();
    
    
    // Wait for the thread, to avoid asserting
    ImportThread.join();
    
    // Unlock
    DoingImport = false;
    UpdateReadyStatus();
}

void Importer::_OnImportProgress(){

    ProgressBar->set_value(100 * ReportedProgress);
}

void Importer::_OnCopyToCollection(){

    StartImporting(false);
}

void Importer::_OnMoveToCollection(){

    StartImporting(true);
}

void Importer::_OnAddImagesFromFolder(){

    Gtk::FileChooserDialog dialog("Choose a folder to scan for images",
        Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    
    dialog.set_transient_for(*this);
    dialog.set_select_multiple(false);

    //Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    // Wait for a selection
    int result = dialog.run();

    //Handle the response:
    switch(result)
    {
    case(Gtk::RESPONSE_OK):
    {
        std::string filename = dialog.get_filename();

        if(filename.empty())
            return;

        FindContent(filename);
        
        if(DeleteImportFoldersIfEmpty->get_active())
            FoldersToDelete.push_back(filename);
        
        return;
    }
    case(Gtk::RESPONSE_CANCEL):
    default:
    {
        // Canceled / nothing selected //
        return;
    }
    }
}

void Importer::_OnBrowseForImages(){

    Gtk::FileChooserDialog dialog("Choose an image to open",
        Gtk::FILE_CHOOSER_ACTION_OPEN);
    
    dialog.set_transient_for(*this);
    dialog.set_select_multiple();

    //Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    //Add filters, so that only certain file types can be selected:
    auto filter_image = Gtk::FileFilter::create();
    filter_image->set_name("Image Files");

    for(const auto& type : SUPPORTED_EXTENSIONS){

        filter_image->add_mime_type(std::get<1>(type));
    }
    dialog.add_filter(filter_image);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    // Wait for a selection
    int result = dialog.run();

    //Handle the response:
    switch(result)
    {
    case(Gtk::RESPONSE_OK):
    {
        std::vector<std::string> files = dialog.get_filenames();

        for(const std::string &file : files)
            FindContent(file);
        
        return;
    }
    case(Gtk::RESPONSE_CANCEL):
    default:
    {
        // Canceled / nothing selected //
        return;
    }
    }
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
