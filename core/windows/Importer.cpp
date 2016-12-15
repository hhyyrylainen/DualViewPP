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

#include "leviathan/Common/StringOperations.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //

Importer::Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    
#ifdef DV_BUILDER_WORKAROUND

    builder->get_widget_derived("PreviewImage", PreviewImage);
    PreviewImage->Init(nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);
    
#else
    
    builder->get_widget_derived("PreviewImage", PreviewImage, nullptr,
        SuperViewer::ENABLED_EVENTS::ALL, false);

#endif // DV_BUILDER_WORKAROUND

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
    CollectionNameCompletion.Init(CollectionName, nullptr,
        std::bind(&Database::SelectCollectionNamesByWildcard, &DualView::Get().GetDatabase(),
            std::placeholders::_1, std::placeholders::_2));

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


    BUILDER_GET_WIDGET(DeselectCurrentImage);
    DeselectCurrentImage->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnDeselectCurrent));
    
    BUILDER_GET_WIDGET(BrowseForward);
    BrowseForward->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnSelectNext));

    BUILDER_GET_WIDGET(BrowseBack);
    BrowseBack->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_OnSelectPrevious));


    Gtk::Button* RemoveSelectedButton;
    builder->get_widget("RemoveSelectedButton", RemoveSelectedButton);
    LEVIATHAN_ASSERT(RemoveSelectedButton, "Invalid .glade file");

    RemoveSelectedButton->signal_clicked().connect(sigc::mem_fun(*this,
            &Importer::_RemoveSelected));
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
    if(CollectionName->get_text().empty())
        CollectionName->set_text(Leviathan::StringOperations::RemovePath(path));

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

    // Find duplicates //
    for(const auto& image : ImagesToImport){

        if(image->GetResourcePath() != file)
            continue;

        LOG_INFO("Importer: adding non-database file twice");
        
        auto dialog = Gtk::MessageDialog(*this,
            "Add the same image again?",
            false,
            Gtk::MESSAGE_QUESTION,
            Gtk::BUTTONS_YES_NO,
            true 
        );

        dialog.set_secondary_text("Image at path: " + file +
            " has already been added to this importer.");
                
        int result = dialog.run();

        if(result != Gtk::RESPONSE_YES){

            return false;
        }
    }

    std::shared_ptr<Image> img;
    
    try{

        img = Image::Create(file);

    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("Failed to add image to importer:");
        e.PrintToLog();
        return false;
    }

    ImagesToImport.push_back(img);
    ImagesToImportOriginalPaths[img.get()] = file;
    _UpdateImageList();
    
    LOG_INFO("Importer added new image: " + file);
    return true;
}

void Importer::_UpdateImageList(){

    ImageList->SetShownItems(ImagesToImport.begin(), ImagesToImport.end(),
        std::make_shared<ItemSelectable>(
            std::bind(&Importer::OnItemSelected, this, std::placeholders::_1)));
}

void Importer::AddExisting(const std::vector<std::shared_ptr<Image>> &images){

    ImagesToImport.reserve(ImagesToImport.size() + images.size());

    for(const auto &image : images)
        ImagesToImport.push_back(image);
    
    _UpdateImageList();
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

    DualView::IsOnMainThreadAssert();

    if(DoingImport){

        StatusLabel->set_text("Import in progress...");
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

    // Check for duplicate hashes //
    bool hashesready = true;
    bool changedimages = false;

    for(auto iter = ImagesToImport.begin(); iter != ImagesToImport.end(); ++iter){

        const auto& image = *iter;

        if(!image->IsReady()){

            hashesready = false;
            continue;
        }

        for(auto iter2 = ImagesToImport.begin(); iter2 != ImagesToImport.end(); ++iter2){

            if(iter == iter2)
                continue;

            const auto& image2 = *iter2;

            if(!image2->IsReady()){

                hashesready = false;
                continue;
            }

            if(image2->GetHash() == image->GetHash() &&
                UserHasAnsweredDeleteQuestion.find(image2->GetResourcePath()) ==
                UserHasAnsweredDeleteQuestion.end() &&
                image->GetResourcePath() != image2->GetResourcePath())
            {

                LOG_INFO("Importer: duplicate images detected");

                auto dialog = Gtk::MessageDialog(*this,
                    "Remove Duplicate Images",
                    false,
                    Gtk::MESSAGE_QUESTION,
                    Gtk::BUTTONS_YES_NO,
                    true 
                );

                dialog.set_secondary_text("Images " +
                    image->GetName() + " at: " + image->GetResourcePath() + "\nand " +
                    image2->GetName() + " at: " + image2->GetResourcePath() +
                    "\nare the same. Delete the second one (will also delete from disk)?");
                
                int result = dialog.run();

                if(result == Gtk::RESPONSE_YES){

                    boost::filesystem::remove(image2->GetResourcePath());
                    ImagesToImport.erase(iter2);

                    // Next duplicate will be found on the recursive call //
                    changedimages = true;
                    break;
                    
                } else {

                    UserHasAnsweredDeleteQuestion[image2->GetResourcePath()] = true;
                }
            }
        }

        if(changedimages)
            break;
    }


    if(changedimages){

        _UpdateImageList();
        UpdateReadyStatus();
        return;
    }
    
    

    if(SelectedImages.empty()){

        StatusLabel->set_text("No images selected");
        PreviewImage->RemoveImage();

    } else {

        if(hashesready){
            
            StatusLabel->set_text("Ready to import " +
                Convert::ToString(SelectedImages.size()) + " images");
            
        } else {
            
            StatusLabel->set_text("Image hashes not ready yet. Selected " +
                Convert::ToString(SelectedImages.size()) + " images");
        }

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
void Importer::_OnDeselectCurrent(){

    ImageList->DeselectFirstItem();
    UpdateReadyStatus();
}

void Importer::_OnSelectNext(){
    
    ImageList->SelectNextItem();
    UpdateReadyStatus();
}

void Importer::_OnSelectPrevious(){

    ImageList->SelectPreviousItem();
    UpdateReadyStatus();
}

void Importer::_RemoveSelected(){

    ImagesToImport.erase(std::remove_if(ImagesToImport.begin(), ImagesToImport.end(),
            [this](auto &x)
            {
                return std::find(SelectedImages.begin(), SelectedImages.end(), x) !=
                    SelectedImages.end();
            }),
        ImagesToImport.end());
            
    _UpdateImageList();
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

    // Require confirmation if adding to uncategorized //
    if(CollectionName->get_text().empty()){

        auto dialog = Gtk::MessageDialog(*this,
            "Import to Uncategorized?",
            false,
            Gtk::MESSAGE_QUESTION,
            Gtk::BUTTONS_YES_NO,
            true 
        );

        dialog.set_secondary_text("Importing to Uncategorized makes finding images "
            "later more difficult.");
        int result = dialog.run();

        if(result != Gtk::RESPONSE_YES){

            ReportedProgress = 1.f;
            _OnImportProgress();
            DoingImport = false;
            return false;
        }
    }

    // If going to move ask to delete already existing images //
    if(move){

        for(auto iter = SelectedImages.begin(); iter != SelectedImages.end(); ++iter){

            if(!(*iter)->IsInDatabase())
                continue;

            // Allow deleting original database one //

            auto found = ImagesToImportOriginalPaths.find((*iter).get());

            if(found != ImagesToImportOriginalPaths.end()){

                std::string pathtodelete = found->second;

                if(!boost::filesystem::exists(pathtodelete))
                    continue;

                auto dialog = Gtk::MessageDialog(*this,
                    "Delete Existing File?",
                    false,
                    Gtk::MESSAGE_QUESTION,
                    Gtk::BUTTONS_YES_NO,
                    true 
                );

                dialog.set_secondary_text("File at: " + pathtodelete +
                    " \nis already in the database with the name: " + (*iter)->GetName() +
                    "\nDelete the file?");
                
                int result = dialog.run();

                if(result == Gtk::RESPONSE_YES){

                    boost::filesystem::remove(pathtodelete);
                }
            }
        }
    }
    
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

    // Wait for the thread, to avoid asserting
    ImportThread.join();

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

            // Could clean stuff from ImagesToImportOriginalPaths but it isn't needed
            
            
            _UpdateImageList();
        }

        // Reset collection tags //
        CollectionTags->Clear();
        CollectionTagsEditor->ReadSetTags();

        if(ImagesToImport.empty())
            CollectionName->set_text("");

        // Reset target folder //
        TargetFolder->GoToRoot();

        // Delete empty folders //
        for(auto iter = FoldersToDelete.begin(); iter != FoldersToDelete.end(); ){

            if(boost::filesystem::is_empty(*iter) && boost::filesystem::is_directory(*iter)){

                LOG_INFO("Importer: deleting empty folder: " + *iter);
                boost::filesystem::remove(*iter);
                iter = FoldersToDelete.erase(iter);
                
            } else {

                ++iter;
            }
        }
        
    } else {

        auto dialog = Gtk::MessageDialog(*this,
            "Failed to import selected images",
            false,
            Gtk::MESSAGE_ERROR,
            Gtk::BUTTONS_OK,
            true 
        );

        dialog.set_secondary_text("Please check the log for more specific errors.");
        dialog.run();

        // Unlock
        DoingImport = false;
        UpdateReadyStatus();
        
        return;
    }
    
    // Reset variables //
    SelectedImages.clear();
    
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
