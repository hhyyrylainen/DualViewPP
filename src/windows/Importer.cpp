// ------------------------------------ //
#include "Importer.h"

#include <boost/filesystem.hpp>

#include "Common.h"
#include "DualView.h"

#include "Common/StringOperations.h"
#include "components/FolderSelector.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"
#include "components/TagEditor.h"
#include "resources/Folder.h"
#include "resources/Image.h"
#include "resources/Tags.h"

#include "Database.h"
#include "TimeIncludes.h"
#include "UtilityHelpers.h"

using namespace DV;

// ------------------------------------ //
Importer::Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window), OpenAlreadyImportedDeleted("Delete Already Imported Files...")
{
    // Primary menu buttons
    OpenAlreadyImportedDeleted.property_relief() = Gtk::RELIEF_NONE;
    OpenAlreadyImportedDeleted.signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnOpenAlreadyImportedDeleter));
    MenuPopover.Container.pack_start(OpenAlreadyImportedDeleted);

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButton", Menu, MenuPopover);

    builder->get_widget_derived("PreviewImage", PreviewImage, nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);

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

    BrowseForImages->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnBrowseForImages));

    Gtk::Button* AddImagesFromFolder;
    builder->get_widget("AddImagesFromFolder", AddImagesFromFolder);
    LEVIATHAN_ASSERT(AddImagesFromFolder, "Invalid .glade file");

    AddImagesFromFolder->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnAddImagesFromFolder));

    Gtk::Button* ReverseImages;
    BUILDER_GET_WIDGET(ReverseImages);
    ReverseImages->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnReverseImages));

    signal_delete_event().connect(sigc::mem_fun(*this, &Importer::_OnClosed));

    // Dropping files into the list //
    std::vector<Gtk::TargetEntry> listTargets;
    listTargets.push_back(Gtk::TargetEntry("text/uri-list"));
    ImageList->drag_dest_set(
        listTargets, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);

    ImageList->signal_drag_data_received().connect(sigc::mem_fun(*this, &Importer::_OnFileDropped));
    ImageList->signal_drag_motion().connect(sigc::mem_fun(*this, &Importer::_OnDragMotion));
    ImageList->signal_drag_drop().connect(sigc::mem_fun(*this, &Importer::_OnDrop));

    builder->get_widget("CollectionName", CollectionName);
    LEVIATHAN_ASSERT(CollectionName, "Invalid .glade file");
    CollectionNameCompletion.Init(CollectionName, nullptr,
        std::bind(&Database::SelectCollectionNamesByWildcard, &DualView::Get().GetDatabase(), std::placeholders::_1,
            std::placeholders::_2));

    Gtk::Button* CopyToCollection;
    builder->get_widget("CopyToCollection", CopyToCollection);
    LEVIATHAN_ASSERT(CopyToCollection, "Invalid .glade file");

    CopyToCollection->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnCopyToCollection));

    Gtk::Button* MoveToCollection;
    builder->get_widget("MoveToCollection", MoveToCollection);
    LEVIATHAN_ASSERT(MoveToCollection, "Invalid .glade file");

    MoveToCollection->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnMoveToCollection));

    // Progress dispatcher
    ProgressDispatcher.connect(sigc::mem_fun(*this, &Importer::_OnImportProgress));

    // Create the collection tag holder
    CollectionTags = std::make_shared<TagCollection>();
    CollectionTagsEditor->SetEditedTags({CollectionTags});

    BUILDER_GET_WIDGET(DeselectCurrentImage);
    DeselectCurrentImage->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnDeselectCurrent));

    BUILDER_GET_WIDGET(BrowseForward);
    BrowseForward->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnSelectNext));

    BUILDER_GET_WIDGET(BrowseBack);
    BrowseBack->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_OnSelectPrevious));

    Gtk::Button* RemoveSelectedButton;
    builder->get_widget("RemoveSelectedButton", RemoveSelectedButton);
    LEVIATHAN_ASSERT(RemoveSelectedButton, "Invalid .glade file");

    RemoveSelectedButton->signal_clicked().connect(sigc::mem_fun(*this, &Importer::_RemoveSelected));
}

Importer::~Importer()
{
    LOG_INFO("Importer properly closed");
    Close();
}

// ------------------------------------ //
void Importer::FindContent(const std::string& path, bool recursive /*= false*/)
{
    namespace bf = boost::filesystem;

    LOG_INFO("Importer finding content from: " + path);

    if (!bf::is_directory(path))
    {
        // A single file //
        _AddImageToList(path);
        return;
    }

    // Set the target collection //
    if (CollectionName->get_text().empty())
        CollectionName->set_text(Leviathan::StringOperations::RemovePath(path));

    std::vector<std::string> foundFiles;

    // Loop contents //
    if (recursive)
    {
        for (bf::recursive_directory_iterator iter(path); iter != bf::recursive_directory_iterator(); ++iter)
        {
            if (bf::is_directory(iter->status()))
                continue;

            foundFiles.push_back(iter->path().string());
        }
    }
    else
    {
        for (bf::directory_iterator iter(path); iter != bf::directory_iterator(); ++iter)
        {
            if (bf::is_directory(iter->status()))
                continue;

            foundFiles.push_back(iter->path().string());
        }
    }

    // Sort the found files
    SortFilePaths(foundFiles.begin(), foundFiles.end());

    // Add the found files to this importer
    for (const auto& file : foundFiles)
    {
        _AddImageToList(file);
    }
}

bool Importer::_AddImageToList(const std::string& file)
{
    if (!DualView::IsFileContent(file))
        return false;

    // Find duplicates //
    for (const auto& image : ImagesToImport)
    {
        if (image->GetResourcePath() != file)
            continue;

        LOG_INFO("Importer: adding non-database file twice");

        auto dialog = Gtk::MessageDialog(
            *this, "Add the same image again?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text("Image at path: " + file + " has already been added to this importer.");

        int result = dialog.run();

        if (result != Gtk::RESPONSE_YES)
        {
            return false;
        }
    }

    std::shared_ptr<Image> img;

    try
    {
        img = Image::Create(file);
    }
    catch (const Leviathan::InvalidArgument& e)
    {
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

void Importer::_UpdateImageList()
{
    ImageList->SetShownItems(ImagesToImport.begin(), ImagesToImport.end(),
        std::make_shared<ItemSelectable>(std::bind(&Importer::OnItemSelected, this, std::placeholders::_1)));
}

void Importer::AddExisting(const std::vector<std::shared_ptr<Image>>& images)
{
    ImagesToImport.reserve(ImagesToImport.size() + images.size());

    for (const auto& image : images)
        ImagesToImport.push_back(image);

    _UpdateImageList();
}

// ------------------------------------ //
bool Importer::_OnClosed(GdkEventAny* event)
{
    _ReportClosed();
    return false;
}

void Importer::_OnClose()
{
    if (DoingImport)
    {
        // Ask user to interrupt importing //
        LOG_WARNING("Importer closing while doing import");
    }

    if (ImportThread.joinable())
        ImportThread.join();

    close();

    // Todo: release logic
}

// ------------------------------------ //
void Importer::UpdateReadyStatus()
{
    DualView::IsOnMainThreadAssert();

    const auto start = std::chrono::high_resolution_clock::now();

    if (DoingImport)
    {
        StatusLabel->set_text("Import in progress...");
        set_sensitive(false);
        return;
    }

    SelectedImages.clear();
    SelectedItems.clear();

    ImageList->GetSelectedItems(SelectedItems);

    for (const auto& preview : SelectedItems)
    {
        auto asImage = std::dynamic_pointer_cast<Image>(preview);

        if (!asImage)
        {
            LOG_WARNING("Importer: SuperContainer has non-image items in it");
            continue;
        }

        SelectedImages.push_back(asImage);
    }

    // Check for duplicate hashes //
    HashesReady = true;
    bool changedimages = false;
    size_t missingHashes = 0;
    bool invalidLoad = false;
    std::string invalidImageName;

    // Recursive call after first deletion will ask if applying the same operation to others should be done
    if (AskingUserPopupQuestions)
    {
        AskDeleteDuplicatesNext = true;
    }
    else
    {
        AskingUserPopupQuestions = true;
        AskedDeleteDuplicatesAlready = false;
        RememberCurrentPromptAnswer = false;
        AskDeleteDuplicatesNext = false;
    }

    for (auto iter = ImagesToImport.begin(); iter != ImagesToImport.end(); ++iter)
    {
        const auto& image = *iter;

        if (!image->IsReady())
        {
            if (image->IsHashInvalid() && !invalidLoad)
            {
                invalidLoad = true;
                invalidImageName = image->GetName();
            }

            HashesReady = false;
            ++missingHashes;
            continue;
        }

        for (auto iter2 = ImagesToImport.begin(); iter2 != ImagesToImport.end(); ++iter2)
        {
            if (iter == iter2)
                continue;

            const auto& image2 = *iter2;

            if (!image2->IsReady())
            {
                HashesReady = false;
                continue;
            }

            if (image2->GetHash() == image->GetHash() &&
                UserHasAnsweredDeleteQuestion.find(image2->GetResourcePath()) == UserHasAnsweredDeleteQuestion.end() &&
                image->GetResourcePath() != image2->GetResourcePath())
            {
                if (!AskedDeleteDuplicatesAlready)
                    LOG_INFO("Importer: duplicate images detected");

                // Ask to remember same operation for all images
                if (AskDeleteDuplicatesNext && !AskedDeleteDuplicatesAlready)
                {
                    AskedDeleteDuplicatesAlready = true;
                    AskDeleteDuplicatesNext = false;

                    auto dialog = Gtk::MessageDialog(*this, "Do Same Operation For All Files?", false,
                        Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

                    dialog.set_secondary_text("Apply same operation to other files as well? (next file is: " +
                        image2->GetResourcePath() + ")");

                    int result = dialog.run();

                    if (result == Gtk::RESPONSE_YES)
                    {
                        RememberCurrentPromptAnswer = true;
                    }
                    else
                    {
                        RememberCurrentPromptAnswer = false;
                    }
                }

                if (RememberCurrentPromptAnswer)
                {
                    if (DeleteDuplicatesAnswer)
                    {
                        LOG_INFO("Remembering delete operation for duplicate: " + image2->GetResourcePath());
                        boost::filesystem::remove(image2->GetResourcePath());
                        ImagesToImport.erase(iter2);

                        // Next duplicate will be found on the recursive call //
                        changedimages = true;
                        break;
                    }

                    continue;
                }

                auto dialog = Gtk::MessageDialog(
                    *this, "Remove Duplicate Images", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

                dialog.set_secondary_text("Images " + image->GetName() + " at: " + image->GetResourcePath() + "\nand " +
                    image2->GetName() + " at: " + image2->GetResourcePath() +
                    "\nare the same. Delete the second one (will also delete from disk)?");

                int result = dialog.run();

                if (result == Gtk::RESPONSE_YES)
                {
                    boost::filesystem::remove(image2->GetResourcePath());
                    ImagesToImport.erase(iter2);

                    // Next duplicate will be found on the recursive call //
                    changedimages = true;
                    DeleteDuplicatesAnswer = true;
                    break;
                }
                else
                {
                    UserHasAnsweredDeleteQuestion[image2->GetResourcePath()] = true;
                    DeleteDuplicatesAnswer = false;
                }
            }
        }

        if (changedimages)
            break;
    }

    // TODO: should this be made non-recursive? At least it seems like that it's hopefully not possible to
    // stackoverflow with reasonable image counts
    if (changedimages)
    {
        _UpdateImageList();
        UpdateReadyStatus();

        AskingUserPopupQuestions = false;
        return;
    }

    AskingUserPopupQuestions = false;

    if (SelectedImages.empty())
    {
        StatusLabel->set_text("No images selected");
        PreviewImage->RemoveImage();
    }
    else
    {
        if (HashesReady && !invalidLoad)
        {
            StatusLabel->set_text("Ready to import " + Convert::ToString(SelectedImages.size()) + " images");
        }
        else if (invalidLoad)
        {
            StatusLabel->set_text("One or more image info / hash compute failed. First invalid: " + invalidImageName);
        }
        else
        {
            StatusLabel->set_text("Image hashes not ready yet (waiting: " + std::to_string(missingHashes) +
                "). Selected " + Convert::ToString(SelectedImages.size()) + " images");
            // TODO: would be nice to have a periodic re-check to see when images are ready
        }

        PreviewImage->SetImage(SelectedImages.front());
    }

    // Tag editing //
    std::vector<std::shared_ptr<TagCollection>> tagstoedit;

    for (const auto& image : SelectedImages)
    {
        tagstoedit.push_back(image->GetTags());
    }

    SelectedImageTags->SetEditedTags(tagstoedit);

    set_sensitive(true);

    if (false)
    {
        LOG_INFO("Importer: UpdateReadyStatus: took: " +
            std::to_string(
                std::chrono::duration_cast<SecondDuration>(std::chrono::high_resolution_clock::now() - start).count()) +
            "s");
    }
}

void Importer::OnItemSelected(ListItem& item)
{
    // Disallow when automatically modifying item list to avoid crashes
    if (AskingUserPopupQuestions)
        return;

    // Deselect others if only one is wanted //
    if (SelectOnlyOneImage->get_active() && item.IsSelected())
    {
        // Deselect all others //
        ImageList->DeselectAllExcept(&item);
    }

    if (SuppressIndividualSelectCallback)
        return;

    UpdateReadyStatus();
}

// ------------------------------------ //
void Importer::_OnDeselectCurrent()
{
    ImageList->DeselectFirstItem();
}

void Importer::_OnSelectNext()
{
    ImageList->SelectNextItem();
}

void Importer::_OnSelectPrevious()
{
    ImageList->SelectPreviousItem();
}

void Importer::_RemoveSelected()
{
    ImagesToImport.erase(
        std::remove_if(ImagesToImport.begin(), ImagesToImport.end(), [this](auto& x)
            { return std::find(SelectedImages.begin(), SelectedImages.end(), x) != SelectedImages.end(); }),
        ImagesToImport.end());

    _UpdateImageList();
    UpdateReadyStatus();
}

// ------------------------------------ //
bool Importer::StartImporting(bool move)
{
    if (!HashesReady)
    {
        auto dialog =
            Gtk::MessageDialog(*this, "Image Hashes Not Ready", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);

        dialog.set_secondary_text("One or more of the selected images doesn't have a hash computed yet. Please try "
                                  "again in a few seconds. You can change image selections to see the new status");
        dialog.run();
        return false;
    }

    bool expected = false;
    if (!DoingImport.compare_exchange_weak(expected, true, std::memory_order_release, std::memory_order_relaxed))
    {
        return false;
    }

    // Value was changed to true //

    if (SelectedImages.empty())
    {
        StatusLabel->set_text("No images selected to import!");
        DoingImport.store(false);
        return false;
    }

    // Set progress //
    ReportedProgress = 0.01f;
    _OnImportProgress();

    // Require confirmation if adding to uncategorized //
    if (CollectionName->get_text().empty())
    {
        auto dialog = Gtk::MessageDialog(
            *this, "Import to Uncategorized?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text("Importing to Uncategorized makes finding images "
                                  "later more difficult.");
        int result = dialog.run();

        if (result != Gtk::RESPONSE_YES)
        {
            ReportedProgress = 1.f;
            _OnImportProgress();
            DoingImport = false;
            return false;
        }
    }

    // If going to move ask to delete already existing images //
    if (move)
    {
        bool first = true;
        bool askedAlready = false;
        AskingUserPopupQuestions = true;
        RememberCurrentPromptAnswer = false;

        for (auto iter = SelectedImages.begin(); iter != SelectedImages.end(); ++iter)
        {
            if (!(*iter)->IsInDatabase())
                continue;

            // Allow deleting original database one //

            auto found = ImagesToImportOriginalPaths.find((*iter).get());

            if (found != ImagesToImportOriginalPaths.end())
            {
                std::string pathtodelete = found->second;

                if (!boost::filesystem::exists(pathtodelete))
                    continue;

                // Ask to remember same operation for all images
                if (!first && !askedAlready)
                {
                    askedAlready = true;

                    auto dialog = Gtk::MessageDialog(*this, "Do Same Operation For All Files?", false,
                        Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

                    dialog.set_secondary_text(
                        "Apply same operation to other files as well? (next file is: " + pathtodelete + ")");

                    int result = dialog.run();

                    if (result == Gtk::RESPONSE_YES)
                    {
                        RememberCurrentPromptAnswer = true;
                    }
                    else
                    {
                        RememberCurrentPromptAnswer = false;
                    }
                }

                if (RememberCurrentPromptAnswer)
                {
                    if (DeleteAlreadyImportedAnswer)
                    {
                        LOG_INFO("Remembering delete operation for: " + pathtodelete);
                        boost::filesystem::remove(pathtodelete);
                    }
                    continue;
                }

                auto dialog = Gtk::MessageDialog(
                    *this, "Delete Existing File?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

                dialog.set_secondary_text("File at: " + pathtodelete +
                    " \nis already in the database with the name: " + (*iter)->GetName() + "\nDelete the file?");

                int result = dialog.run();

                if (result == Gtk::RESPONSE_YES)
                {
                    boost::filesystem::remove(pathtodelete);
                    DeleteAlreadyImportedAnswer = true;
                }
                else
                {
                    DeleteAlreadyImportedAnswer = false;
                }

                first = false;
            }
        }

        AskingUserPopupQuestions = false;
    }

    // Run import in a new thread //
    ImportThread = std::thread(&Importer::_RunImportThread, this, CollectionName->get_text(), move);

    // Update selected //
    UpdateReadyStatus();
    // Because DoingImport is true the above function only sets this to be not-sensitive

    return true;
}

void Importer::_RunImportThread(const std::string& collection, bool move)
{
    bool result = DualView::Get().AddToCollection(SelectedImages, move, collection, *CollectionTags,
        [this](float progress)
        {
            ReportedProgress = progress;
            ProgressDispatcher.emit();
        });

    // Invoke onfinish on the main thread //
    auto isalive = GetAliveMarker();

    DualView::Get().InvokeFunction(
        [this, result, isalive]()
        {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            _OnImportFinished(result);
        });
}

void Importer::_OnImportFinished(bool success)
{
    LEVIATHAN_ASSERT(DualView::IsOnMainThread(), "_OnImportFinished called on the wrong thread");

    ReportedProgress = 1.f;
    _OnImportProgress();

    // Wait for the thread, to avoid asserting
    ImportThread.join();

    // Remove images if succeeded //
    if (success)
    {
        // Add the collection to the target folder //
        auto targetfolder = TargetFolder->GetFolder();

        if (!targetfolder->IsRoot())
        {
            DualView::Get().AddCollectionToFolder(
                targetfolder, DualView::Get().GetDatabase().SelectCollectionByNameAG(CollectionName->get_text()));
        }

        LOG_INFO(std::string("Import (to: ") + CollectionName->get_text().c_str() + ") was successful");

        if (RemoveAfterAdding->get_active())
        {
            // Remove SelectedImages from ImagesToImport
            ImagesToImport.erase(
                std::remove_if(ImagesToImport.begin(), ImagesToImport.end(), [this](auto& x)
                    { return std::find(SelectedImages.begin(), SelectedImages.end(), x) != SelectedImages.end(); }),
                ImagesToImport.end());

            // Could clean stuff from ImagesToImportOriginalPaths but it isn't needed

            _UpdateImageList();
        }

        // Reset collection tags //
        CollectionTags->Clear();
        CollectionTagsEditor->ReadSetTags();

        if (ImagesToImport.empty())
            CollectionName->set_text("");

        // Reset target folder //
        if (!TargetFolder->TargetPathLockedIn())
            TargetFolder->GoToRoot();

        // Delete empty folders //
        for (auto iter = FoldersToDelete.begin(); iter != FoldersToDelete.end();)
        {
            bool remove = false;

            try
            {
                if (boost::filesystem::is_directory(*iter) && boost::filesystem::is_empty(*iter))
                {
                    remove = true;
                }
            }
            catch (const boost::filesystem::filesystem_error& e)
            {
                LOG_WARNING("Couldn't check folder (" + *iter + ") for emptiness: " + e.what());
            }

            if (remove)
            {
                LOG_INFO("Importer: deleting empty folder: " + *iter);
                boost::filesystem::remove(*iter);
                iter = FoldersToDelete.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }
    else
    {
        auto dialog = Gtk::MessageDialog(
            *this, "Failed to import selected images", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);

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

void Importer::_OnImportProgress()
{
    ProgressBar->set_value(100 * ReportedProgress);
}

void Importer::_OnCopyToCollection()
{
    StartImporting(false);
}

void Importer::_OnMoveToCollection()
{
    StartImporting(true);
}

void Importer::_OnAddImagesFromFolder()
{
    Gtk::FileChooserDialog dialog("Choose a folder to scan for images", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);

    dialog.set_transient_for(*this);
    dialog.set_select_multiple(false);

    // Add response buttons the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    // Wait for a selection
    int result = dialog.run();

    // Handle the response:
    switch (result)
    {
        case (Gtk::RESPONSE_OK):
        {
            std::string filename = dialog.get_filename();

            if (filename.empty())
                return;

            FindContent(filename);

            if (DeleteImportFoldersIfEmpty->get_active())
                FoldersToDelete.push_back(filename);

            return;
        }
        case (Gtk::RESPONSE_CANCEL):
        default:
        {
            // Canceled / nothing selected //
            return;
        }
    }
}

void Importer::_OnReverseImages()
{
    // Disallow doing this while importing
    if (DoingImport.load(std::memory_order_consume))
        return;

    if (ImagesToImport.empty())
        return;

    bool hasSelected = ImageList->CountSelectedItems() > 0;

    // Tuple of image, original position
    std::vector<std::tuple<std::shared_ptr<Image>, size_t>> toSwap;

    // Reverse images, if selected only reverse the selected images
    if (hasSelected)
    {
        std::vector<std::shared_ptr<ResourceWithPreview>> toSwapRaw;
        ImageList->GetSelectedItems(toSwapRaw);

        for (const auto& item : toSwapRaw)
        {
            auto asImage = std::dynamic_pointer_cast<Image>(item);

            if (!asImage)
                continue;

            toSwap.emplace_back(asImage, -1);
        }
    }
    else
    {
        for (const auto& image : ImagesToImport)
        {
            toSwap.emplace_back(image, -1);
        }
    }

    // Calculate original positions
    for (auto& tuple : toSwap)
    {
        bool found = false;

        size_t i;
        for (i = 0; i < ImagesToImport.size(); ++i)
        {
            if (ImagesToImport[i] == std::get<0>(tuple))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG_ERROR("Failed to find original position for image to reorder");
            return;
        }

        std::get<1>(tuple) = i;
    }

    // Swap items around
    for (size_t i = 0; i < toSwap.size(); ++i)
    {
        const auto swapWith = toSwap.size() - 1 - i;

        if (swapWith == i)
            continue;

        const auto& from = toSwap[i];
        const auto toIndex = std::get<1>(toSwap[swapWith]);

        bool found = false;

        size_t source;
        for (source = 0; source < ImagesToImport.size(); ++source)
        {
            if (ImagesToImport[source] == std::get<0>(from))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG_ERROR("Failed to find original position for image to move (partial move may have happened)");
            return;
        }

        std::swap(ImagesToImport[source], ImagesToImport[toIndex]);
    }

    // Update the display from the source data
    _UpdateImageList();
    UpdateReadyStatus();
}

void Importer::_OnOpenAlreadyImportedDeleter()
{
    DualView::Get().OpenAlreadyImportedDeleteWindow();
    MenuPopover.hide();
}

void Importer::_OnBrowseForImages()
{
    Gtk::FileChooserDialog dialog("Choose an image to open", Gtk::FILE_CHOOSER_ACTION_OPEN);

    dialog.set_transient_for(*this);
    dialog.set_select_multiple();

    // Add response buttons the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    // Add filters, so that only certain file types can be selected:
    auto filter_image = Gtk::FileFilter::create();
    filter_image->set_name("Image Files");

    for (const auto& type : SUPPORTED_EXTENSIONS)
    {
        filter_image->add_mime_type(std::get<1>(type));
    }
    dialog.add_filter(filter_image);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    // Wait for a selection
    int result = dialog.run();

    // Handle the response:
    switch (result)
    {
        case (Gtk::RESPONSE_OK):
        {
            std::vector<std::string> files = dialog.get_filenames();

            for (const std::string& file : files)
                FindContent(file);

            return;
        }
        case (Gtk::RESPONSE_CANCEL):
        default:
        {
            // Canceled / nothing selected //
            return;
        }
    }
}

// ------------------------------------ //
void Importer::_OnDeselectAll()
{
    SuppressIndividualSelectCallback = true;
    ImageList->DeselectAllItems();

    SuppressIndividualSelectCallback = false;
    UpdateReadyStatus();
}

void Importer::_OnSelectAll()
{
    SuppressIndividualSelectCallback = true;

    // If the "select only one" checkbox is checked this doesn't work properly
    if (SelectOnlyOneImage->get_active())
    {
        SelectOnlyOneImage->set_active(false);

        ImageList->SelectAllItems();

        SelectOnlyOneImage->set_active(true);
    }
    else
    {
        ImageList->SelectAllItems();
    }

    SuppressIndividualSelectCallback = false;
    UpdateReadyStatus();
}

// ------------------------------------ //
bool Importer::_OnDragMotion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
    if (DoingImport)
    {
        context->drag_refuse(time);
        return false;
    }

    const auto suggestion = context->get_suggested_action();

    context->drag_status(suggestion == Gdk::ACTION_MOVE ? suggestion : Gdk::ACTION_COPY, time);
    return true;
}

bool Importer::_OnDrop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
    if (DoingImport)
    {
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

        for (const auto& uri : uriList)
        {
            Glib::ustring path = Glib::filename_from_uri(uri);

            FindContent(path);
        }

        context->drag_finish(true, false, time);
        return;
    }

    context->drag_finish(false, false, time);
}

// ------------------------------------ //
