#pragma once

#include "Common/ThreadSafe.h"
#include "SQLHelpers.h"

#include "PreparedStatement.h"
#include "SingleLoad.h"

#include "Common.h"

#include <string>
#include <vector>

#include <thread>

// Forward declare sqlite //
struct sqlite3;

namespace DV {

class PreparedStatement;

class CurlWrapper;

class Image;
class Collection;
class Folder;
class TagCollection;
class NetGallery;
class NetFile;
class DatabaseAction;
class ImageDeleteAction;
class ImageMergeAction;
class ImageDeleteFromCollectionAction;
class CollectionReorderAction;
class NetGalleryDeleteAction;
class CollectionDeleteAction;

class Tag;
class AppliedTag;
class TagModifier;
class TagBreakRule;

enum class DATABASE_ACTION_TYPE : int;

// The version number of the database
constexpr auto DATABASE_CURRENT_VERSION = 26;
constexpr auto DATABASE_CURRENT_SIGNATURES_VERSION = 1;

constexpr auto IMAGE_SIGNATURE_WORD_COUNT = 100;
constexpr auto IMAGE_SIGNATURE_WORD_LENGTH = 10;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
//! \note This is recursively lockable because otherwise the acrobatics needed to pass around
//! the single lock is way too much
class Database : public Leviathan::ThreadSafeRecursive {

    //! \brief Holds data in Database::SqliteExecGrabResult
    struct GrabResultHolder {

        struct Row {

            std::vector<std::string> ColumnValues;
            std::vector<std::string> ColumnNames;
        };

        size_t MaxRows = 0;
        std::vector<Row> Rows;
    };

    // These actions are friends in order to call the UndoAction and RedoAction methods
    friend ImageDeleteAction;
    friend ImageMergeAction;
    friend ImageDeleteFromCollectionAction;
    friend CollectionReorderAction;
    friend NetGalleryDeleteAction;
    friend CollectionDeleteAction;

public:
    //! \brief Normal database creation, uses the specified file
    Database(std::string dbfile);

    //! \brief Creates an in-memory database for testing, the tests parameter must be
    //! true
    Database(bool tests);

    //! \brief Closes the database safely
    //! \todo Forcefully orphan every active object in the SingleLoad holders
    ~Database();

    //! \brief Must be called before using the database. Setups all the tables
    //! \exception Leviathan::InvalidState if the database setup fails
    //! \note Must be called on the thread that
    void Init();

    //! \brief Selects the database version
    //! \returns True if succeeded, false if no version exists.
    bool SelectDatabaseVersion(LockT& guard, sqlite3* db, int& result);

    //! \brief Removes objects that no longer have external references from the database
    //! \todo Call this periodically from the database thread in DualView
    void PurgeInactiveCache();

    //
    // Image functions
    //

    //! \brief Inserts a new image to the database
    //!
    //! Also sets the id in the image object
    //! \exception InvalidSQL if the image violates unique constraints (same hash)
    void InsertImage(LockT& guard, Image& image);

    //! \brief Updates an images properties in the database
    //! \returns False if the update fails
    bool UpdateImage(LockT& guard, Image& image);
    CREATE_NON_LOCKING_WRAPPER(UpdateImage);

    //! \brief Deletes an image from the database
    //! \returns A database action that can undo the operation if it succeeded
    std::shared_ptr<DatabaseAction> DeleteImage(Image& image);

    //! \brief Only creates the action to delete an image but doesn't execute it
    //! \note This clears the cache entries from the signature DB so it isn't completely
    //! accurate that this doesn't do anything yet
    std::shared_ptr<ImageDeleteAction> CreateDeleteImageAction(LockT& guard, Image& image);

    //! \brief Finds the newest ImageDeleteAction that contains the image
    std::shared_ptr<ImageDeleteAction> SelectImageDeleteActionForImage(
        Image& image, bool performed);

    //! \brief Retrieves an Image based on the hash
    //! \todo All callers need to check if the image is deleted or not
    std::shared_ptr<Image> SelectImageByHash(LockT& guard, const std::string& hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByHash);

    //! \brief Retrieves an Image based on the id
    std::shared_ptr<Image> SelectImageByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByID);

    //! \brief Retrieves an Image based on the id but doesn't retrieve deleted images
    std::shared_ptr<Image> SelectImageByIDSkipDeleted(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByIDSkipDeleted);

    //! \brief Retrieves images based on tags
    std::vector<std::shared_ptr<Image>> SelectImageByTag(LockT& guard, DBID tagid);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByTag);

    //! \brief Retrieves an Image's id based on the hash
    DBID SelectImageIDByHash(LockT& guard, const std::string& hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDByHash);

    //! \brief Return Image name by ID
    std::string SelectImageNameByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectImageNameByID);


    //! \brief Retrieves signature (or empty string) for image id
    std::string SelectImageSignatureByID(LockT& guard, DBID image);
    CREATE_NON_LOCKING_WRAPPER(SelectImageSignatureByID);

    //! \brief Retrieves image ids that don't have a signature
    std::vector<DBID> SelectImageIDsWithoutSignature(LockT& guard);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDsWithoutSignature);

    //! \brief Loads a TagCollection for the specified image.
    //! \returns Null if the image is not in the database
    std::shared_ptr<TagCollection> LoadImageTags(const std::shared_ptr<Image>& image);

    //! \brief Retrieves all tags added to an image
    void SelectImageTags(LockT& guard, std::weak_ptr<Image> image,
        std::vector<std::shared_ptr<AppliedTag>>& tags);

    //! \brief Adds a tag to an image
    void InsertImageTag(LockT& guard, std::weak_ptr<Image> image, AppliedTag& tag);

    //! \brief Removes a tag from an image
    void DeleteImageTag(LockT& guard, std::weak_ptr<Image> image, AppliedTag& tag);

    //! \brief Queries the signature words table to find potentially duplicate images
    //! \param sensitivity How many parts need to match before returning a result. Strength of
    //! 20 can be used instead of refining the results further to get a faster method for
    //! finding duplicates, but less accurate
    //! \result Returns the images that have duplicates (lower ids are reported as the original
    //! Image). The vector containing the tuples first has the id of the duplicate Image and
    //! then the strength
    std::map<DBID, std::vector<std::tuple<DBID, int>>> SelectPotentialImageDuplicates(
        int sensitivity = 15);

    //
    // Collection functions
    //

    //! \brief Checks if a collection name is in use
    //! \param name The name to check
    //! \param ignoreDuplicateId If set, a collection with the specified ID is ignored
    //! \returns True if in use
    bool CheckIsCollectionNameInUse(
        LockT& guard, const std::string& name, int ignoreDuplicateId = -1);
    CREATE_NON_LOCKING_WRAPPER(CheckIsCollectionNameInUse);

    //! \brief Inserts a new collection to the database
    //! \exception InvalidSQL if the collection violates unique constraints (same name)
    std::shared_ptr<Collection> InsertCollection(
        LockT& guard, const std::string& name, bool isprivate);
    CREATE_NON_LOCKING_WRAPPER(InsertCollection);

    //! \brief Updates a collection's properties in the database
    //! \returns False if the update fails
    bool UpdateCollection(LockT& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(UpdateCollection);

    //! \brief Deletes n collection from the database
    //! \returns A database action that can undo the operation if it succeeded
    std::shared_ptr<DatabaseAction> DeleteCollection(Collection& collection);

    //! \brief Only creates the action to delete a collection but doesn't execute it
    std::shared_ptr<CollectionDeleteAction> CreateDeleteCollectionAction(
        LockT& guard, Collection& collection);

    //! \brief Finds the newest ImageDeleteAction that contains the image
    std::shared_ptr<CollectionDeleteAction> SelectCollectionDeleteAction(
        Collection& collection, bool performed);

    //! \brief Retrieves a Collection based on the name
    //! \todo Once aliases are implemented this should also check aliases
    std::shared_ptr<Collection> SelectCollectionByName(LockT& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionByName);

    std::string SelectCollectionNameByID(DBID id);

    std::vector<std::string> SelectCollectionNamesByWildcard(
        const std::string& pattern, int64_t max = 50);

    //! \brief Retrieves a Collection based on the id
    std::shared_ptr<Collection> SelectCollectionByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionByID);

    //! \brief Returns the largest value used for an image in the collection
    //! If the collection is empty returns 0
    int64_t SelectCollectionLargestShowOrder(LockT& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionLargestShowOrder);

    //! \brief Returns the number of images in a collection
    int64_t SelectCollectionImageCount(LockT& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionImageCount);

    //! \brief Loads a TagCollection for the specified collection.
    //! \returns Null if the collection is not in the database
    std::shared_ptr<TagCollection> LoadCollectionTags(
        const std::shared_ptr<Collection>& collection);

    //! \brief Retrieves all tags added to an collection
    void SelectCollectionTags(LockT& guard, std::weak_ptr<Collection> collection,
        std::vector<std::shared_ptr<AppliedTag>>& tags);

    //! \brief Adds a tag to an collection
    void InsertCollectionTag(
        LockT& guard, std::weak_ptr<Collection> collection, AppliedTag& tag);

    //! \brief Removes a tag from an collection
    void DeleteCollectionTag(
        LockT& guard, std::weak_ptr<Collection> collection, AppliedTag& tag);

    //
    // Collection image
    //

    //! \brief Adds image to collection
    //! \returns True if succeeded. False if no rows where changed
    //! \param showorder The order number of the image in the collection. If the same one is
    //! already in use the order in which the two images that have the same show_order is
    //! random
    bool InsertImageToCollection(
        LockT& guard, DBID collection, Image& image, int64_t showorder);
    CREATE_NON_LOCKING_WRAPPER(InsertImageToCollection);

    bool InsertImageToCollection(
        LockT& guard, Collection& collection, Image& image, int64_t showorder);

    bool InsertImageToCollection(
        LockT& guard, Collection& collection, DBID image, int64_t showorder);

    bool InsertImageToCollection(LockT& guard, DBID collection, DBID image, int64_t showorder);

    //! \brief Returns true if image is in some collection.
    //!
    //! If false it should be added to one otherwise it cannot be viewed
    bool SelectIsImageInAnyCollection(LockT& guard, const Image& image);
    CREATE_NON_LOCKING_WRAPPER(SelectIsImageInAnyCollection);

    bool SelectIsImageInCollection(LockT& guard, DBID collection, DBID image);

    int64_t SelectCollectionCountImageIsIn(LockT& guard, const Image& image);

    //! \brief Removes an image from the collection
    //! \returns True if removed. False if the image wasn't in the collection
    //!
    //! The image will be added to uncategorized if it ends up in no collection
    //! This is a variant that can't be undone
    bool DeleteImageFromCollection(LockT& guard, Collection& collection, Image& image);

    //! \brief Removes images from a collection in a way that can be undone
    std::shared_ptr<DatabaseAction> DeleteImagesFromCollection(
        Collection& collection, const std::vector<std::shared_ptr<Image>>& images);

    //! \brief Finds images that are only in collection and returns their IDs
    std::vector<DBID> SelectImagesThatWouldBecomeOrphanedWhenRemovedFromCollection(
        LockT& guard, Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectImagesThatWouldBecomeOrphanedWhenRemovedFromCollection);

    std::vector<DBID> SelectImagesThatWouldBecomeOrphanedWhenRemovedFromCollection(
        LockT& guard, DBID collection);

    //! \brief Returns the show_order image has in collection. Or -1
    int64_t SelectImageShowOrderInCollection(
        LockT& guard, const Collection& collection, const Image& image);
    CREATE_NON_LOCKING_WRAPPER(SelectImageShowOrderInCollection);

    //! \brief Returns the image with the show order
    //! \note If for some reason there are multiples with the same show_order one of them
    //! is returned
    std::shared_ptr<Image> SelectImageInCollectionByShowOrder(
        LockT& guard, const Collection& collection, int64_t showorder);
    CREATE_NON_LOCKING_WRAPPER(SelectImageInCollectionByShowOrder);

    //! \returns Image ID at show order in collection or -1 if not found
    DBID SelectImageIDInCollectionByShowOrder(
        LockT& guard, DBID collection, int64_t showorder);

    //! \brief It's possible to end up with multiple Images with the same show order, this
    //! function can be used to still get all the images in that situation
    std::vector<std::shared_ptr<Image>> SelectImagesInCollectionByShowOrder(
        LockT& guard, const Collection& collection, int64_t showorder);

    //! \brief Returns the preview image for a collection
    std::shared_ptr<Image> SelectCollectionPreviewImage(const Collection& collection);

    //! \brief Returns the first image in a collection, or null if empty
    std::shared_ptr<Image> SelectFirstImageInCollection(
        LockT& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectFirstImageInCollection);

    //! \brief Returns the last image in a collection, or null if empty
    std::shared_ptr<Image> SelectLastImageInCollection(
        LockT& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectLastImageInCollection);

    //! \brief Counts the index image has in a collection based on the show_orders
    //! \todo This doesn't handle deleted but SelectImageInCollectionByShowIndex does so this
    //! acts weird. Fix this
    int64_t SelectImageShowIndexInCollection(const Collection& collection, const Image& image);

    //! \brief Returns the image with the show_order index
    //! \note If for some reason there are multiples with the same show_order one of them
    //! is returned
    std::shared_ptr<Image> SelectImageInCollectionByShowIndex(
        LockT& guard, const Collection& collection, int64_t index);
    CREATE_NON_LOCKING_WRAPPER(SelectImageInCollectionByShowIndex);

    //! \brief Selects the image that has the next show order
    std::shared_ptr<Image> SelectNextImageInCollectionByShowOrder(
        const Collection& collection, int64_t showorder);

    //! \brief Selects the image that has the previous show order
    std::shared_ptr<Image> SelectPreviousImageInCollectionByShowOrder(
        const Collection& collection, int64_t showorder);

    //! \brief Returns all images in a collection
    std::vector<std::shared_ptr<Image>> SelectImagesInCollection(
        const Collection& collection, int32_t limit = -1);

    std::vector<std::tuple<DBID, int64_t>> SelectImageIDsAndShowOrderInCollection(
        const Collection& collection);

    //! \brief Returns all collections and the show order an image has
    std::vector<std::tuple<DBID, int64_t>> SelectCollectionIDsImageIsIn(
        LockT& guard, const Image& image);

    std::vector<std::tuple<DBID, int64_t>> SelectCollectionIDsImageIsIn(
        LockT& guard, DBID image);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionIDsImageIsIn);

    //! \brief Used to push back images in a collection to make space in the middle
    //! \returns Number of changed rows
    int UpdateShowOrdersInCollection(
        LockT& guard, DBID collection, int64_t startpoint, int incrementby = 1);

    //! \brief Updates the show order for a single image
    //!
    //! This does no checks to ensure that this doesn't cause duplicate show orders
    bool UpdateCollectionImageShowOrder(
        LockT& guard, DBID collection, DBID image, int64_t showorder);

    //! \brief Updates the order of collection images in an action
    std::shared_ptr<DatabaseAction> UpdateCollectionImagesOrder(
        Collection& collection, const std::vector<std::shared_ptr<Image>>& neworder);

    //
    // Folder
    //
    //! \todo Make this cache the result after the first call to improve performance
    std::shared_ptr<Folder> SelectRootFolder(LockT& guard);
    CREATE_NON_LOCKING_WRAPPER(SelectRootFolder);

    //! \brief Returns a folder by id
    std::shared_ptr<Folder> SelectFolderByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectFolderByID);

    //! \brief Creates a new folder, must have a parent folder
    //! \returns The created folder or null if the name conflicts
    //! \todo Make sure that if there are any quotes they are balanced
    std::shared_ptr<Folder> InsertFolder(
        std::string name, bool isprivate, const Folder& parent);

    bool UpdateFolder(LockT& guard, Folder& folder);

    //
    // Folder collection
    //
    bool InsertCollectionToFolder(LockT& guard, Folder& folder, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(InsertCollectionToFolder);

    void DeleteCollectionFromFolder(Folder& folder, const Collection& collection);

    //! \brief Returns Collections that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Collection>> SelectCollectionsInFolder(
        const Folder& folder, const std::string& matchingpattern = "");

    //! \brief Returns true if collection is in a Folder
    bool SelectCollectionIsInFolder(LockT& guard, const Collection& collection);

    //! \brief Returns true if Collection is in another folder than folder
    //! \todo This doesn't handle deleted properly
    bool SelectCollectionIsInAnotherFolder(
        LockT& guard, const Folder& folder, const Collection& collection);

    //! \brief Returs Folders Collection is in
    //! \todo This doesn't handle deleted properly
    std::vector<DBID> SelectFoldersCollectionIsIn(const Collection& collection);

    //! \brief Deletes a Collection from the root folder if the collection is in another
    //! folder.
    //! \todo This doesn't handle deleted properly
    void DeleteCollectionFromRootIfInAnotherFolder(const Collection& collection);

    //! \brief Adds a Collection to root if it isn't in any folder
    //! \todo This doesn't handle deleted properly
    void InsertCollectionToRootIfInNone(const Collection& collection);

    //
    // Folder folder
    //

    //! \brief Adds a folder to folder
    //! \exception InvalidSQL If fails
    //! \note This doesn't check for name conflicts
    void InsertFolderToFolder(LockT& guard, Folder& folder, const Folder& parent);

    //! \brief Inserts a folder to the root folder, if it currently isn't in any folder
    void InsertToRootFolderIfInNoFolders(LockT& guard, Folder& folder);

    //! \brief Deletes a folder from parent
    //!
    //! Doesn't add to root folder even if the folder is in no folder after this
    bool DeleteFolderFromFolder(LockT& guard, Folder& folder, const Folder& parent);

    //! \brief Counts in how many folders the folder is
    int64_t SelectFolderParentCount(LockT& guard, Folder& folder);

    //! \brief Returns Folders that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Folder>> SelectFoldersInFolder(
        const Folder& folder, const std::string& matchingpattern = "");

    //! \brief Selects a folder based on parent folder and name
    std::shared_ptr<Folder> SelectFolderByNameAndParent(
        LockT& guard, const std::string& name, const Folder& parent);
    CREATE_NON_LOCKING_WRAPPER(SelectFolderByNameAndParent);

    //! \brief Selects all parents of a Folder
    std::vector<DBID> SelectFolderParents(const Folder& folder);

    //! \brief Finds the first parent folder of the given folder where name is in use
    //!
    //! This can be used to detect name conflicts with a folder
    //! \return The parent folder where the first conflict (in random order) was found, null if
    //! no conflict
    std::shared_ptr<Folder> SelectFirstParentFolderWithChildFolderNamed(
        LockT& guard, const Folder& parentsOf, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectFirstParentFolderWithChildFolderNamed);

    //
    // Tag
    //

    //! \brief Creates a new tag
    std::shared_ptr<Tag> InsertTag(
        std::string name, std::string description, TAG_CATEGORY category, bool isprivate);

    //! \brief Returns tag based on id
    std::shared_ptr<Tag> SelectTagByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByID);

    std::shared_ptr<Tag> SelectTagByName(LockT& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByName);

    //! \brief Selects tags containing pattern
    std::vector<std::shared_ptr<Tag>> SelectTagsWildcard(
        const std::string& pattern, int64_t max = 50, bool aliases = true);

    //! \brief Selects a tag based on an alias name
    std::shared_ptr<Tag> SelectTagByAlias(LockT& guard, const std::string& alias);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByAlias);

    //! \brief Selects a tag matching the name or has an alias for the name
    std::shared_ptr<Tag> SelectTagByNameOrAlias(const std::string& name);

    //! \brief Returns the text of a super alias
    std::string SelectTagSuperAlias(const std::string& name);

    //! \brief Updates Tag's properties
    void UpdateTag(Tag& tag);

    //! \brief Adds an alias to a tag
    //! \returns True if added, false if the alias is already in use
    bool InsertTagAlias(Tag& tag, const std::string& alias);

    //! \brief Deletes a tag alias
    void DeleteTagAlias(const std::string& alias);

    //! \brief Deletes a tag alias if it is an alias for tag
    void DeleteTagAlias(const Tag& tag, const std::string& alias);

    //! \brief Returns aliases that point to tag
    std::vector<std::string> SelectTagAliases(const Tag& tag);

    //! \brief Adds an imply to a tag
    //! \returns True if added, false if the alias is already in use
    bool InsertTagImply(Tag& tag, const Tag& implied);

    //! \brief Removes an implied tag from tag
    //! \returns True if added, false if the alias is already in use
    void DeleteTagImply(Tag& tag, const Tag& implied);

    //! \brief Returns the tags implied by tag
    std::vector<std::shared_ptr<Tag>> SelectTagImpliesAsTag(const Tag& tag);

    //! \brief Returns the ids of tags implied by tag
    std::vector<DBID> SelectTagImplies(LockT& guard, const Tag& tag);


    //
    // AppliedTag
    //
    std::shared_ptr<AppliedTag> SelectAppliedTagByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectAppliedTagByID);

    //! \brief If the tag is in the database (same main tag and other properties)
    //! returns that
    //!
    //! Used when inserting to prefer using existing tags instead of creating a new AppliedTag
    //! for every image and every tag combination
    std::shared_ptr<AppliedTag> SelectExistingAppliedTag(LockT& guard, const AppliedTag& tag);
    CREATE_NON_LOCKING_WRAPPER(SelectExistingAppliedTag);

    //! \brief Returns an existing tag with the same properties as tag
    //! \returns -1 when tag doesn't exist, id on success
    DBID SelectExistingAppliedTagID(LockT& guard, const AppliedTag& tag);
    CREATE_NON_LOCKING_WRAPPER(SelectExistingAppliedTagID);

    std::vector<std::shared_ptr<TagModifier>> SelectAppliedTagModifiers(
        LockT& guard, const AppliedTag& appliedtag);

    //! \brief Returns the right side of a tag combine, appliedtag is the left side
    std::tuple<std::string, std::shared_ptr<AppliedTag>> SelectAppliedTagCombine(
        LockT& guard, const AppliedTag& appliedtag);

    //! \brief Inserts a new AppliedTag to the database
    //! \todo Break this into smaller functions
    bool InsertAppliedTag(LockT& guard, AppliedTag& tag);

    //! \brief Deletes an AppliedTag if it isn't used anymore
    //! \todo Check should this only be called when cleaning up the databse
    //! as there probably won't be infinitely many tags that aren't used
    void DeleteAppliedTagIfNotUsed(LockT& guard, AppliedTag& tag);

    //! \brief Returns true if applied tag id is referenced somewhere
    bool SelectIsAppliedTagUsed(LockT& guard, DBID id);

    //! \brief Checks that the TagModifiers set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagModifiersMatch(LockT& guard, DBID id, const AppliedTag& tag);

    //! \brief Checks that the tag combines set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagCombinesMatch(LockT& guard, DBID id, const AppliedTag& tag);

    //! \brief Helper for looping all applied_tag
    //! \returns -1 on error
    DBID SelectAppliedTagIDByOffset(LockT& guard, int64_t offset);

    //! \brief Combines two applied tags into one
    //!
    //! Used after checking that to applied_tag entries match to remove the duplicate.
    //! First is the ID that will be preserved and all references to second will be changed
    //! to first
    void CombineAppliedTagDuplicate(LockT& guard, DBID first, DBID second);



    //
    // TagModifier
    //
    std::shared_ptr<TagModifier> SelectTagModifierByID(LockT& guard, DBID id);

    std::shared_ptr<TagModifier> SelectTagModifierByName(
        LockT& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByName);

    std::shared_ptr<TagModifier> SelectTagModifierByAlias(
        LockT& guard, const std::string& alias);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByAlias);

    std::shared_ptr<TagModifier> SelectTagModifierByNameOrAlias(
        LockT& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByNameOrAlias);


    void UpdateTagModifier(const TagModifier& modifier);

    //
    // TagBreakRule
    //
    //! \brief Returns a break rule that can handle str (or might be able to)
    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByStr(const std::string& str);

    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByExactPattern(
        LockT& guard, const std::string& pattern);

    std::vector<std::shared_ptr<TagModifier>> SelectModifiersForBreakRule(
        LockT& guard, const TagBreakRule& rule);

    //! \todo Implement
    void UpdateTagBreakRule(const TagBreakRule& rule);

    //
    // NetGallery
    //

    //! \brief Returns all the netgalleries
    //! \param nodownloaded If true won't include galleries that are marked as downloaded
    std::vector<DBID> SelectNetGalleryIDs(bool nodownloaded);

    //! \brief Returns a NetGallery by id
    std::shared_ptr<NetGallery> SelectNetGalleryByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectNetGalleryByID);

    //! \brief Adds a new NetGallery to the database
    //! \returns True if added, false if it is already added
    bool InsertNetGallery(LockT& guard, std::shared_ptr<NetGallery> gallery);

    void UpdateNetGallery(NetGallery& gallery);

    //! \brief Deletes n NetGallery from the database
    //! \returns A database action that can undo the operation if it succeeded
    std::shared_ptr<DatabaseAction> DeleteNetGallery(NetGallery& gallery);

    //
    // NetFile
    //

    //! \brief Returns all the netgalleries
    //! \param nodownloaded If true won't include galleries that are marked as downloaded
    std::vector<std::shared_ptr<NetFile>> SelectNetFilesFromGallery(NetGallery& gallery);

    //! \brief Returns a NetFile by id
    std::shared_ptr<NetFile> SelectNetFileByID(LockT& guard, DBID id);

    //! \brief Adds a new NetFile to the database
    void InsertNetFile(LockT& guard, NetFile& netfile, NetGallery& gallery);

    void UpdateNetFile(NetFile& netfile);

    //! \brief Deletes a NetFile
    //!
    //! NetFiles are always related to a single gallery so the only way to remove them is to
    //! delete them
    //! \todo Allow reversing this operation
    void DeleteNetFile(NetFile& netfile);

    //
    // Wilcard searches for tag suggestions
    //
    //! \brief Returns text of all break rules that contain str
    void SelectTagBreakRulesByStrWildcard(
        std::vector<std::string>& breakrules, const std::string& pattern);

    void SelectTagNamesWildcard(std::vector<std::string>& result, const std::string& pattern);

    void SelectTagAliasesWildcard(
        std::vector<std::string>& result, const std::string& pattern);

    void SelectTagModifierNamesWildcard(
        std::vector<std::string>& result, const std::string& pattern);

    void SelectTagSuperAliasWildcard(
        std::vector<std::string>& result, const std::string& pattern);

    //
    // Complex operations
    //
    //! \brief Merges specified images into the target image
    //!
    //! This will copy all non-duplicate tags from the merged images and add the target to
    //! collections from the merged images that it wasn't in.
    //! \returns A performed action that can be used to undo the action
    std::shared_ptr<DatabaseAction> MergeImages(
        const Image& mergetarget, const std::vector<std::shared_ptr<Image>>& tomerge);


    //
    // Actions
    //
    std::shared_ptr<DatabaseAction> SelectDatabaseActionByID(LockT& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectDatabaseActionByID);


    std::shared_ptr<DatabaseAction> SelectOldestDatabaseAction(LockT& guard);

    //! \brief Adds a new action to the DB
    //!
    //! May not be called with an action that is already in the DB
    void InsertDatabaseAction(LockT& guard, DatabaseAction& action);


    //! \brief Returns the latest actions matching the optional search string
    //! \param search String that must be found either in the json data or the description of
    //! an action
    //! \param limit The max number of items returned. -1 is unlimited
    std::vector<std::shared_ptr<DatabaseAction>> SelectLatestDatabaseActions(
        const std::string& search = "", int limit = -1);

    //! \brief Updates the JSON data and Performed of the action
    bool UpdateDatabaseAction(LockT& guard, DatabaseAction& action);
    CREATE_NON_LOCKING_WRAPPER(UpdateDatabaseAction);

    //! \brief Permanently deletes an action
    //!
    //! Will also permanently delete all resources assocciated with the action
    void DeleteDatabaseAction(DatabaseAction& action);

    //! \brief Purges old actions while the number of actions is over MaxActions
    void PurgeOldActionsUntilUnderLimit(LockT& guard);

    //! \brief Purges old actions while the number of actions is over actionstokeep
    void PurgeOldActionsUntilSpecificCount(LockT& guard, uint32_t actionstokeep);
    CREATE_NON_LOCKING_WRAPPER(PurgeOldActionsUntilSpecificCount);

    //! \brief Updates the maximum action count
    //!
    //! Doesn't immediately purge old actions only applies when a new action is performed
    //! \param maxactions Then number of actions to keep. Must be 1 or more otherwise the
    //! action purging and undo mechanism breaks
    void SetMaxActionHistory(uint32_t maxactions);

    //
    // Ignore pairs
    //
    void InsertIgnorePairs(const std::vector<std::tuple<DBID, DBID>>& pairs);

    void DeleteIgnorePairs(const std::vector<std::tuple<DBID, DBID>>& pairs);

    void DeleteAllIgnorePairs();

    //
    // Database maintenance functions
    //
    //! \brief Finds all AppliedTags that have the same properties and combines them
    void CombineAllPossibleAppliedTags(LockT& guard);

    //! \brief Returns the number of rows in applied_tag
    int64_t CountAppliedTags();

    void GenerateMissingDatabaseActionDescriptions(LockT& guard);

    //! \brief Tries to escape quotes in a string for insertion to sql statements
    static std::string EscapeSql(std::string str);

    //! \brief Helper callback for standard operations
    //! \warning The user parameter has to be a pointer to Database::GrabResultHolder
    static int SqliteExecGrabResult(
        void* user, int columns, char** columnsastext, char** columnname);


    // Statement grouping //

    //! \brief Begins a transaction that queues all database actions until a CommitTransaction
    //! call.
    //!
    //! The database must be locked until the transaction is committed
    //! \param alsoauxiliary If true a transaction is also started on the secondary databases
    //! \see CommitTransaction
    void BeginTransaction(LockT& guardLocked, bool alsoauxiliary = false);

    void CommitTransaction(LockT& guardLocked, bool alsoauxiliary = false);

    void RollbackTransaction(LockT& guardLocked, bool alsoauxiliary = false);

    //! \brief Alternative to transaction that can be nested
    //!
    //! Must be accompanied by a ReleaseSavePoint call
    //! \warning The savepoint name is not sent as a prepared statement, SQL injection is
    //! possible!
    void BeginSavePoint(
        LockT& guard, const std::string& savepointname, bool alsoauxiliary = false);

    void ReleaseSavePoint(
        LockT& guard, const std::string& savepointname, bool alsoauxiliary = false);

    void RollbackSavePoint(
        LockT& guard, const std::string& savepointname, bool alsoauxiliary = false);

    //! \brief Returns true if a transaction is in progress
    bool HasActiveTransaction(LockT& guard);

    // Statistics functions //
    size_t CountExistingTags();
    size_t CountDatabaseActions();

protected:
    // These are for DatabaseAction to use
    void RedoAction(ImageDeleteAction& action);
    void UndoAction(ImageDeleteAction& action);
    void PurgeAction(ImageDeleteAction& action);

    void RedoAction(ImageMergeAction& action);
    void UndoAction(ImageMergeAction& action);
    void PurgeAction(ImageMergeAction& action);

    void RedoAction(ImageDeleteFromCollectionAction& action);
    void UndoAction(ImageDeleteFromCollectionAction& action);

    void RedoAction(CollectionReorderAction& action);
    void UndoAction(CollectionReorderAction& action);

    void RedoAction(NetGalleryDeleteAction& action);
    void UndoAction(NetGalleryDeleteAction& action);
    void PurgeAction(NetGalleryDeleteAction& action);

    void RedoAction(CollectionDeleteAction& action);
    void UndoAction(CollectionDeleteAction& action);
    void PurgeAction(CollectionDeleteAction& action);

protected:
    //! \brief Runs a command and prints all the result rows with column headers to log
    void PrintResultingRows(LockT& guard, sqlite3* db, const std::string& str);

    //! \brief Runs a command and prints all the result rows with column headers to log
    //!
    //! This version allows settings parameters
    template<typename... TBindTypes>
    void PrintResultingRows(
        LockT& guard, sqlite3* db, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(db, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        LOG_INFO("SQL result from: \"" + str + "\"");
        statementobj.StepAndPrettyPrint(statementinuse);
    }

    //! \brief Runs SQL statement as a prepared statement with the values
    template<typename... TBindTypes>
    void RunSQLAsPrepared(LockT& guard, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(SQLiteDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        statementobj.StepAll(statementinuse);
    }


    //! \brief Runs SQL statement as a prepared statement with the values on the signature DB
    template<typename... TBindTypes>
    void RunOnSignatureDB(LockT& guard, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(PictureSignatureDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        statementobj.StepAll(statementinuse);
    }

    //! \brief Runs a raw sql query.
    //! \note Don't use unless absolutely necessary prefer to use Database::RunSqlAsPrepared
    void _RunSQL(LockT& guard, const std::string& sql);

    //! \brief Variant for auxiliary dbs
    void _RunSQL(LockT& guard, sqlite3* db, const std::string& sql);

    //! \brief Throws an InvalidSQL exception, filling it with values from the database
    //! connection
    void ThrowCurrentSqlError(LockT& guard);

private:
    //
    // Row parsing functions
    //

    std::shared_ptr<NetFile> _LoadNetFileFromRow(LockT& guard, PreparedStatement& statement);

    std::shared_ptr<NetGallery> _LoadNetGalleryFromRow(
        LockT& guard, PreparedStatement& statement);

    //! \brief Loads a TagBreakRule object from the current row
    std::shared_ptr<TagBreakRule> _LoadTagBreakRuleFromRow(
        LockT& guard, PreparedStatement& statement);

    //! \brief Loads an AppliedTag object from the current row
    std::shared_ptr<AppliedTag> _LoadAppliedTagFromRow(
        LockT& guard, PreparedStatement& statement);

    //! \brief Loads a TagModifier object from the current row
    std::shared_ptr<TagModifier> _LoadTagModifierFromRow(
        LockT& guard, PreparedStatement& statement);

    //! \brief Loads a Tag object from the current row
    std::shared_ptr<Tag> _LoadTagFromRow(LockT& guard, PreparedStatement& statement);

    //! \brief Loads a Collection object from the current row
    std::shared_ptr<Collection> _LoadCollectionFromRow(
        LockT& guard, PreparedStatement& statement);

    //! \brief Loads an Image object from the current row
    std::shared_ptr<Image> _LoadImageFromRow(LockT& guard, PreparedStatement& statement);

    //! \brief Loads a Folder object from the current row
    std::shared_ptr<Folder> _LoadFolderFromRow(LockT& guard, PreparedStatement& statement);

    //! \brief Loads a DatabaseAction derived object from the current row
    std::shared_ptr<DatabaseAction> _LoadDatabaseActionFromRow(
        LockT& guard, PreparedStatement& statement);

    //
    // Private insert stuff
    //
    void InsertTagImage(LockT& guard, Image& image, DBID appliedtagid);

    void InsertTagCollection(LockT& guard, Collection& image, DBID appliedtagid);

    //! \brief Inserts the image signature to the auxiliary DB
    void _InsertImageSignatureParts(LockT& guard, DBID image, const std::string& signature);

    //
    // Helper operations
    //
    void _SetActionStatus(LockT& guard, DatabaseAction& action, bool performed);

    void _PurgeImages(LockT& guard, const std::vector<DBID>& images);
    void _PurgeNetGalleries(LockT& guard, DBID gallery);
    void _PurgeCollection(LockT& guard, DBID collection);

    //
    // Utility stuff
    //

    //! \brief Creates default tables and also calls _InsertDefaultTags
    void _CreateTableStructure(LockT& guard);

    //! \brief Variant for the picture signature auxiliary db
    void _CreateTableStructureSignatures(LockT& guard);

    //! \brief Verifies that the specified version is compatible with the current version
    bool _VerifyLoadedVersion(LockT& guard, int fileversion);

    //! \brief Variant for the picture signature auxiliary db
    bool _VerifyLoadedVersionSignatures(LockT& guard, int fileversion);

    //! \brief Called if a loaded database version is older than DATABASE_CURRENT_VERSION
    //! \param oldversion The version from which the update is done, will contain the new
    //! version after this returns. Can be called again to update to the next version
    //! \returns True if succeeded, false if update is not supported
    bool _UpdateDatabase(LockT& guard, const int oldversion);

    //! \brief Variant for the picture signature auxiliary db
    bool _UpdateDatabaseSignatures(LockT& guard, const int oldversion);

    //! \brief Sets the database version. Should only be called from _UpdateDatabase
    void _SetCurrentDatabaseVersion(LockT& guard, int newversion);

    //! \brief Variant for the picture signature auxiliary db
    void _SetCurrentDatabaseVersionSignatures(LockT& guard, int newversion);

    //! \brief Helper for updates. Might be useful integrity checks later
    //! \note This can only be ran while doing a database update because the database
    //! needs to parse tags and to do that it needs to be unlocked and it might mess with
    //! the result that is being iterated
    void _UpdateApplyDownloadTagStrings(LockT& guard);

protected:
    sqlite3* SQLiteDb = nullptr;

    //! This has the image signature table
    sqlite3* PictureSignatureDb = nullptr;

    //! Used for backups before potentially dangerous operations
    std::string DatabaseFile;

    //! Number of actions to keep
    uint32_t ActionsToKeep = 50;

    //! Makes sure each Collection is only loaded once
    SingleLoad<Collection, int64_t> LoadedCollections;

    //! Makes sure each Image is only loaded once
    SingleLoad<Image, int64_t> LoadedImages;

    //! Makes sure each Folder is only loaded once
    SingleLoad<Folder, int64_t> LoadedFolders;

    //! Makes sure each Tag is only loaded once
    SingleLoad<Tag, int64_t> LoadedTags;

    //! Makes sure each NetGallery is only loaded once
    SingleLoad<NetGallery, int64_t> LoadedNetGalleries;

    //! Makes sure each DatabaseAction is only loaded once
    SingleLoad<DatabaseAction, int64_t> LoadedDatabaseActions;
};

//! \brief Helper class that automatically commits a transaction when it destructs
//!\ todo Would be nice to be able to conditionally create these
//! \todo Add a way for this to rollback on exception
class DoDBTransaction {
public:
    DoDBTransaction(Database& db, RecursiveLock& dblock, bool alsoauxiliary = false);

    DoDBTransaction(DoDBTransaction&& other) = delete;
    DoDBTransaction(const DoDBTransaction& other) = delete;
    DoDBTransaction& operator=(const DoDBTransaction& other) = delete;

    ~DoDBTransaction();

    //! \brief Prevent commit on scope exit before end
    void AllowCommit(bool success)
    {
        Success = success;
    }

private:
    Database& DB;
    RecursiveLock& Locked;
    bool Auxiliary;

    bool Success = true;
};

//! \brief Helper class that automatically handles a savepoint
//!\ see DoDBTransaction
//! \warning The savepoint name is not sent as a prepared statement, SQL injection is possible!
class DoDBSavePoint {
public:
    DoDBSavePoint(Database& db, RecursiveLock& dblock, const std::string& savepoint,
        bool alsoauxiliary = false);

    DoDBSavePoint(DoDBSavePoint&& other) = delete;
    DoDBSavePoint(const DoDBSavePoint& other) = delete;
    DoDBSavePoint& operator=(const DoDBSavePoint& other) = delete;

    ~DoDBSavePoint();

    //! \brief Prevent commit on scope exit before end
    void AllowCommit(bool success)
    {
        Success = success;
    }

private:
    Database& DB;
    RecursiveLock& Locked;
    const std::string SavePoint;
    bool Auxiliary;

    bool Success = true;
};

} // namespace DV
