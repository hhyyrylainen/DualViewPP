// ------------------------------------ //
#include "Collection.h"

#include "Common.h"

#include "Database.h"
#include "PreparedStatement.h"
#include "components/CollectionListItem.h"

#include "Database.h"
#include "DualView.h"

#include "Common/StringOperations.h"
#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
// Non-database testing version
Collection::Collection(const std::string& name) :
    DatabaseResource(true), Name(name),

    AddDate(date::make_zoned(
        date::current_zone(), std::chrono::time_point_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()))),

    ModifyDate(AddDate), LastView(AddDate)
{
    // Remove extra spaces
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(Name);
}

Collection::Collection(
    Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db),

    AddDate(TimeHelpers::GetStaleZonedTime()), ModifyDate(AddDate), LastView(AddDate)
{
    // Load properties //
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "add_date");
    CheckRowID(statement, 3, "modify_date");
    CheckRowID(statement, 4, "last_view");
    CheckRowID(statement, 5, "is_private");
    CheckRowID(statement, 6, "preview_image");

    Name = statement.GetColumnAsString(1);
    IsPrivate = statement.GetColumnAsBool(5);

    AddDate = TimeHelpers::ParseTime(statement.GetColumnAsString(2));
    ModifyDate = TimeHelpers::ParseTime(statement.GetColumnAsString(3));
    LastView = TimeHelpers::ParseTime(statement.GetColumnAsString(4));
}

Collection::~Collection()
{
    DBResourceDestruct();
}

// ------------------------------------ //
std::string Collection::GetNameForFolder() const
{
    LEVIATHAN_ASSERT(!Name.empty(), "GetNameForFolder called when Name is empty");

    std::string sanitized = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(
        Name, "\\/<>:\"|", ' ');

    // And then get rid of all characters under 0x1F
    for(auto iter = sanitized.begin(); iter != sanitized.end(); ++iter) {

        if((*iter) <= 0x1F)
            (*iter) = ' ';
    }

    // Remove preceeding and trailing spaces
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(sanitized);

    // Also may not be only dots //
    if(sanitized.find_first_not_of('.') == std::string::npos) {

        sanitized = "padded_" + sanitized;
    }

    // May not end with a dot
    if(sanitized.back() == '.')
        sanitized += "d";

    // Don't start with a dot or an hyphen //
    if(sanitized.front() == '.' || sanitized.front() == '-')
        sanitized = "d" + sanitized;

    // Verify that it is a valid name //
    if(!boost::filesystem::windows_name(sanitized)) {

        LOG_FATAL("Failed to sanitize file name: '" + sanitized + "' is not valid");
        return "";
    }

    return sanitized;
}
// ------------------------------------ //
bool Collection::AddTags(const TagCollection& tags, DatabaseLockT& dblock)
{
    auto currenttags = GetTags();

    if(!currenttags)
        return false;

    currenttags->Add(tags, dblock);
    return true;
}

std::shared_ptr<TagCollection> Collection::GetTags()
{
    if(!Tags && IsInDatabase()) {
        Tags = InDatabase->LoadCollectionTags(shared_from_this());
    }

    return Tags;
}

int64_t Collection::GetLastShowOrder() const
{
    if(!IsInDatabase())
        return 0;

    GUARD_LOCK_OTHER(InDatabase);
    return InDatabase->SelectCollectionLargestShowOrder(guard, *this);
}

int64_t Collection::GetLastShowOrder(DatabaseLockT& dblock) const
{
    if(!IsInDatabase())
        return 0;

    return InDatabase->SelectCollectionLargestShowOrder(dblock, *this);
}
// ------------------------------------ //
bool Collection::AddImage(std::shared_ptr<Image> image)
{
    if(!image || !IsInDatabase())
        return false;

    return InDatabase->InsertImageToCollectionAG(*this, *image, GetLastShowOrder() + 1);
}

bool Collection::AddImage(std::shared_ptr<Image> image, DatabaseLockT& dblock)
{
    if(!image || !IsInDatabase())
        return false;

    return InDatabase->InsertImageToCollection(
        dblock, *this, *image, GetLastShowOrder(dblock) + 1);
}

bool Collection::AddImage(std::shared_ptr<Image> image, int64_t order)
{
    if(!image || !IsInDatabase())
        return false;

    return InDatabase->InsertImageToCollectionAG(*this, *image, order);
}

bool Collection::AddImage(std::shared_ptr<Image> image, int64_t order, DatabaseLockT& dblock)
{
    if(!image || !IsInDatabase())
        return false;

    return InDatabase->InsertImageToCollection(dblock, *this, *image, order);
}

bool Collection::RemoveImage(std::shared_ptr<Image> image, DatabaseLockT& dblock)
{
    if(!image || !IsInDatabase())
        return false;

    return InDatabase->DeleteImageFromCollection(dblock, *this, *image);
}

int64_t Collection::GetImageCount() const
{
    if(!IsInDatabase())
        return 0;

    return InDatabase->SelectCollectionImageCountAG(*this);
}

int64_t Collection::GetImageCount(DatabaseLockT& dblock) const
{
    if(!IsInDatabase())
        return 0;

    return InDatabase->SelectCollectionImageCount(dblock, *this);
}

int64_t Collection::GetImageShowOrder(std::shared_ptr<Image> image) const
{
    if(!image || !IsInDatabase())
        return -1;

    return InDatabase->SelectImageShowOrderInCollectionAG(*this, *image);
}

int64_t Collection::GetImageShowOrder(
    std::shared_ptr<Image> image, DatabaseLockT& dblock) const
{
    if(!image || !IsInDatabase())
        return -1;

    return InDatabase->SelectImageShowOrderInCollection(dblock, *this, *image);
}

std::shared_ptr<Image> Collection::GetPreviewIcon() const
{
    if(!IsInDatabase())
        return nullptr;

    return InDatabase->SelectCollectionPreviewImage(*this);
}

std::vector<std::shared_ptr<Image>> Collection::GetImages() const
{
    if(!IsInDatabase())
        return {};

    return InDatabase->SelectImagesInCollection(*this);
}

// ------------------------------------ //
void Collection::_DoSave(Database& db)
{
    db.UpdateCollection(*this);
}

bool Collection::operator==(const Collection& other) const
{
    if(static_cast<const DatabaseResource&>(*this) ==
        static_cast<const DatabaseResource&>(other)) {
        return true;
    }

    return Name == other.Name;
}
// ------------------------------------ //
// Implementation of ResourceWithPreview
std::shared_ptr<ListItem> Collection::CreateListItem(
    const std::shared_ptr<ItemSelectable>& selectable)
{
    auto widget = std::make_shared<CollectionListItem>(selectable);

    _FillWidget(*widget);

    return widget;
}

bool Collection::IsSame(const ResourceWithPreview& other)
{
    auto* asThis = dynamic_cast<const Collection*>(&other);

    if(!asThis)
        return false;

    // Check is the folder same //
    return *this == *asThis;
}

bool Collection::UpdateWidgetWithValues(ListItem& control)
{
    auto* asOurType = dynamic_cast<CollectionListItem*>(&control);

    if(!asOurType)
        return false;

    // Update the properties //
    _FillWidget(*asOurType);
    return true;
}

void Collection::_FillWidget(CollectionListItem& widget)
{
    widget.SetCollection(shared_from_this());
    widget.Deselect();
}
// ------------------------------------ //
// Implementation of ImageListScroll
std::shared_ptr<Image> Collection::GetNextImage(
    std::shared_ptr<Image> current, bool wrap /*= true*/)
{
    if(!current || !IsInDatabase())
        return nullptr;

    const auto order = InDatabase->SelectImageShowOrderInCollectionAG(*this, *current);

    auto previous = InDatabase->SelectNextImageInCollectionByShowOrder(*this, order);

    if(!previous && wrap)
        previous = InDatabase->SelectFirstImageInCollectionAG(*this);

    return previous;
}

std::shared_ptr<Image> Collection::GetPreviousImage(
    std::shared_ptr<Image> current, bool wrap /*= true*/)
{
    if(!current || !IsInDatabase())
        return nullptr;

    const auto order = InDatabase->SelectImageShowOrderInCollectionAG(*this, *current);

    auto next = InDatabase->SelectPreviousImageInCollectionByShowOrder(*this, order);

    if(!next && wrap)
        next = InDatabase->SelectLastImageInCollectionAG(*this);

    return next;
}
// ------------------------------------ //
bool Collection::HasCount() const
{
    return true;
}

size_t Collection::GetCount() const
{
    return GetImageCount();
}

bool Collection::SupportsRandomAccess() const
{
    return true;
}

std::shared_ptr<Image> Collection::GetImageAt(size_t index) const
{
    if(!IsInDatabase())
        return nullptr;

    return InDatabase->SelectImageInCollectionByShowIndexAG(*this, index);
}

size_t Collection::GetImageIndex(Image& image) const
{
    if(!IsInDatabase())
        return -1;

    return InDatabase->SelectImageShowIndexInCollection(*this, image);
}
// ------------------------------------ //
std::string Collection::GetDescriptionStr() const
{
    return "collection '" + Name + "'";
}
