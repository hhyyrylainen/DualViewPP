// ------------------------------------ //
#include "Image.h"

#include "components/ImageListItem.h"

#include "Tags.h"

#include "DualView.h"

#include "Database.h"
#include "PreparedStatement.h"

#include "CacheManager.h"
#include "Common.h"
#include "Common/StringOperations.h"
#include "Exceptions.h"
#include "FileSystem.h"

#include "base64.h"

#include <boost/filesystem.hpp>

using namespace DV;

// ------------------------------------ //
Image::Image(const std::string& file) :
    DatabaseResource(true), ResourcePath(file),

    AddDate(date::make_zoned(
        date::current_zone(), std::chrono::time_point_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()))),
    LastView(AddDate), ImportLocation(file)

{
    if(!boost::filesystem::exists(file)) {
        OnConstructorFailed();
        throw Leviathan::InvalidArgument("Image: file doesn't exist: " + file);
    }

    ResourceName = boost::filesystem::path(ResourcePath).filename().string();
    Extension = boost::filesystem::path(ResourcePath).extension().string();

    Tags = std::make_shared<TagCollection>();
}

Image::Image(
    const std::string& file, const std::string& name, const std::string& importoverride) :
    DatabaseResource(true),
    ResourcePath(file),

    AddDate(date::make_zoned(
        date::current_zone(), std::chrono::time_point_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()))),
    LastView(AddDate), ImportLocation(importoverride)

{
    if(!boost::filesystem::exists(file)) {
        OnConstructorFailed();
        throw Leviathan::InvalidArgument("Image: file doesn't exist");
    }

    ResourceName = name;
    Extension = boost::filesystem::path(ResourcePath).extension().string();

    Tags = std::make_shared<TagCollection>();
}

Image::Image() :
    DatabaseResource(true), AddDate(date::make_zoned(date::current_zone(),
                                std::chrono::time_point_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now()))),
    LastView(AddDate)
{
    Tags = std::make_shared<TagCollection>();
}


Image::Image(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db), AddDate(TimeHelpers::GetStaleZonedTime()), LastView(AddDate)
{
    IsReadyToAdd = true;
    IsHashValid = true;

    // Load properties //
    CheckRowID(statement, 1, "relative_path");
    CheckRowID(statement, 2, "width");
    CheckRowID(statement, 3, "height");
    CheckRowID(statement, 4, "name");
    CheckRowID(statement, 5, "extension");
    CheckRowID(statement, 6, "add_date");
    CheckRowID(statement, 7, "last_view");
    CheckRowID(statement, 8, "is_private");
    CheckRowID(statement, 9, "from_file");
    CheckRowID(statement, 10, "file_hash");
    CheckRowID(statement, 11, "deleted");


    // Convert path to runtime path
    ResourcePath = CacheManager::GetFinalImagePath(statement.GetColumnAsString(1));
    ResourceName = statement.GetColumnAsString(4);

    Extension = statement.GetColumnAsString(5);

    IsPrivate = statement.GetColumnAsBool(8);


    ImportLocation = statement.GetColumnAsString(9);

    Hash = statement.GetColumnAsString(10);

    Height = statement.GetColumnAsInt(3);
    Width = statement.GetColumnAsInt(2);

    AddDate = TimeHelpers::ParseTime(statement.GetColumnAsString(6));
    LastView = TimeHelpers::ParseTime(statement.GetColumnAsString(7));

    Deleted = statement.GetColumnAsOptionalBool(11);
}

Image::~Image()
{
    DBResourceDestruct();
}

void Image::Init()
{
    if(!IsHashValid) {
        // Register hash calculation //
        _QueueHashCalculation();
    }

    if(!Tags && IsInDatabase()) {

        // Load tags //
        Tags = InDatabase->LoadImageTags(shared_from_this());
    }
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> Image::GetImage()
{
    LEVIATHAN_ASSERT(!ResourcePath.empty(), "Image: ResourcePath is empty");
    return DualView::Get().GetCacheManager().LoadFullImage(ResourcePath);
}

std::shared_ptr<LoadedImage> Image::GetThumbnail()
{
    if(HashCalculateAttempted && !IsHashValid) {
        // Hash attempted but has failed
        return DualView::Get().GetCacheManager().CreateImageLoadFailure(HashError);

    } else if(!IsHashValid) {
        // Hash not ready yet
        return nullptr;
    }

    LEVIATHAN_ASSERT(!ResourcePath.empty(), "Image: ResourcePath is empty");
    return DualView::Get().GetCacheManager().LoadThumbImage(ResourcePath, Hash);
}

std::string Image::GetHash() const
{
    if(!IsHashValid)
        throw Leviathan::InvalidState("Hash hasn't been calculated");

    return Hash;
}
// ------------------------------------ //
void Image::SetResourcePath(const std::string& newpath)
{
    if(!boost::filesystem::exists(newpath)) {

        throw Leviathan::InvalidArgument("Image: update path: file doesn't exist");
    }

    ResourcePath = newpath;
    Extension = boost::filesystem::path(ResourcePath).extension().string();

    OnMarkDirty();
}

void Image::SetSignature(const std::string& signature)
{
    if(Signature == signature)
        return;

    Signature = signature;
    SignatureRetrieved = true;
    OnMarkDirty();
}

const std::string& Image::GetSignature()
{
    GUARD_LOCK();

    if(SignatureRetrieved)
        return Signature;

    if(IsInDatabase()) {

        Signature = InDatabase->SelectImageSignatureByIDAG(ID);
    }

    SignatureRetrieved = true;
    return Signature;
}

std::string Image::GetSignatureBase64()
{
    return base64_encode(GetSignature());
}
// ------------------------------------ //
void Image::_DoSave(Database& db)
{
    db.UpdateImageAG(*this);
}

void Image::_DoSave(Database& db, DatabaseLockT& dblock)
{
    db.UpdateImage(dblock, *this);
}

void Image::_OnAdopted()
{
    // Reload tags //
    Tags = InDatabase->LoadImageTags(shared_from_this());

    // Reset signature status
    SignatureRetrieved = false;
}

void Image::_OnPurged()
{
    if(boost::filesystem::exists(ResourcePath)) {

        try {
            boost::filesystem::remove(ResourcePath);
            LOG_INFO("Image: deleted from disk: " + ResourcePath);
        } catch(const boost::filesystem::filesystem_error& e) {
            LOG_ERROR(
                "Image: failed to delete file (" + ResourcePath + ") from disk: " + e.what());
        }
    }

    ResourcePath = "[deleted]";
    Deleted = true;
}
// ------------------------------------ //
std::string Image::CalculateFileHash() const
{
    LEVIATHAN_ASSERT(!ResourcePath.empty(), "Image: ResourcePath is empty");

    // Load file bytes //
    std::string contents;
    if(!Leviathan::FileSystem::ReadFileEntirely(ResourcePath, contents)) {
        // TODO: if the file was just deleted, this shouldn't fail
        LEVIATHAN_ASSERT(0, "Failed to read file for hash calculation");
    }

    return DualView::CalculateBase64EncodedHash(contents);
}

void Image::_DoHashCalculation()
{
    Hash = CalculateFileHash();

    LEVIATHAN_ASSERT(!Hash.empty(), "Image created an empty hash");
    LEVIATHAN_ASSERT(!ResourcePath.empty(), "Image: ResourcePath is empty");

    // Load the image size //
    if(!CacheManager::GetImageSize(ResourcePath, Width, Height, Extension)) {

        HashError = "Failed to get image size from: " + ResourcePath;
        LOG_ERROR(HashError);

        // This image needs to be destroyed
        Hash = "invalid";
        IsHashValid = false;
        IsValid = false;
        HashCalculateAttempted = true;
        return;
    }

    LEVIATHAN_ASSERT(!Extension.empty(), "File extension is empty");

    IsHashValid = true;
    HashCalculateAttempted = true;

    // IsReadyToAdd will be set by DualView once it is confirmed that
    // this isn't a duplicate
}

void Image::_OnFinishHash()
{
    if(IsHashValid)
        IsReadyToAdd = true;

    // Image size is now available
    // Unless IsHashValid is false
    GUARD_LOCK();
    NotifyAll(guard);
}
// ------------------------------------ //
void Image::_QueueHashCalculation()
{
    DualView::Get().QueueImageHashCalculate(shared_from_this());
}
// ------------------------------------ //
void Image::BecomeDuplicateOf(const Image& other)
{
    LEVIATHAN_ASSERT(other.IsHashValid, "Image becoming duplicate of invalid hash");

    if(other.IsDeleted()) {
        LOG_WARNING("Image(" + ResourcePath + "): becoming duplicate of deleted image id:" +
                    std::to_string(other.GetID()));
    }


    // Copy database ID //
    _BecomeDuplicateOf(other);

    ResourcePath = other.ResourcePath;
    ResourceName = other.ResourceName;
    Extension = other.Extension;
    IsPrivate = other.IsPrivate;
    AddDate = other.AddDate;
    LastView = other.LastView;
    ImportLocation = other.ImportLocation;

    IsHashValid = true;
    HashCalculateAttempted = true;
    HashError.clear();
    Hash = other.Hash;

    Height = other.Height;
    Width = other.Width;

    Deleted = other.Deleted;

    std::shared_ptr<TagCollection> currentTags = Tags;

    // If the other is from the database we need to load a new TagCollection object
    // because we need a pointer to us to make sure that the weak_ptr doesn't die in the bound
    // parameters
    if(IsInDatabase()) {

        _OnAdopted();

    } else {
        Tags = other.Tags;
    }

    // Apply tags //
    if(currentTags && currentTags->HasTags())
        Tags->Add(*currentTags);

    IsReadyToAdd = true;
}

bool Image::operator==(const Image& other) const
{
    if(static_cast<const DatabaseResource&>(*this) ==
        static_cast<const DatabaseResource&>(other)) {
        return true;
    }

    return ResourcePath == other.ResourcePath;
}
// ------------------------------------ //
std::shared_ptr<ListItem> Image::CreateListItem(
    const std::shared_ptr<ItemSelectable>& selectable)
{
    auto widget = std::make_shared<ImageListItem>(selectable);

    _FillWidget(*widget);

    return widget;
}

bool Image::IsSame(const ResourceWithPreview& other)
{
    auto* asThis = dynamic_cast<const Image*>(&other);

    if(!asThis)
        return false;

    // Check is the image same //
    return *this == *asThis;
}

bool Image::UpdateWidgetWithValues(ListItem& control)
{
    auto* asOurType = dynamic_cast<ImageListItem*>(&control);

    if(!asOurType)
        return false;

    // Update the properties //
    _FillWidget(*asOurType);
    return true;
}

void Image::_FillWidget(ImageListItem& widget)
{
    widget.SetImage(shared_from_this());
    widget.Deselect();

    // TODO: deleted flag
}
