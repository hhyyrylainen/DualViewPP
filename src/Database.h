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

class Tag;
class AppliedTag;
class TagModifier;
class TagBreakRule;

//! \brief The version number of the database
constexpr auto DATABASE_CURRENT_VERSION = 22;
constexpr auto DATABASE_CURRENT_SIGNATURES_VERSION = 1;

constexpr auto IMAGE_SIGNATURE_WORD_COUNT = 100;
constexpr auto IMAGE_SIGNATURE_WORD_LENGTH = 10;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
class Database : public Leviathan::ThreadSafe {

    //! \brief Holds data in Database::SqliteExecGrabResult
    struct GrabResultHolder {

        struct Row {

            std::vector<std::string> ColumnValues;
            std::vector<std::string> ColumnNames;
        };

        size_t MaxRows = 0;
        std::vector<Row> Rows;
    };

    //! \brief Helper class for creating prepared statements


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
    bool SelectDatabaseVersion(Lock& guard, sqlite3* db, int& result);

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
    void InsertImage(Lock& guard, Image& image);

    //! \brief Updates an images properties in the database
    //! \returns False if the update fails
    bool UpdateImage(Lock& guard, Image& image);
    CREATE_NON_LOCKING_WRAPPER(UpdateImage);

    //! \brief Deletes an image from the database
    //!
    //! The image object's id will be set to -1
    //! \returns True if succeeded
    bool DeleteImage(Image& image);

    //! \brief Retrieves an Image based on the hash
    std::shared_ptr<Image> SelectImageByHash(Lock& guard, const std::string& hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByHash);

    //! \brief Retrieves an Image based on the id
    std::shared_ptr<Image> SelectImageByID(Lock& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByID);

    //! \brief Retrieves images based on tags
    std::vector<std::shared_ptr<Image>> SelectImageByTag(Lock& guard, DBID tagid);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByTag);

    //! \brief Retrieves an Image's id based on the hash
    DBID SelectImageIDByHash(Lock& guard, const std::string& hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDByHash);

    //! \brief Retrieves signature (or empty string) for image id
    std::string SelectImageSignatureByID(Lock& guard, DBID image);
    CREATE_NON_LOCKING_WRAPPER(SelectImageSignatureByID);

    //! \brief Retrieves image ids that don't have a signature
    std::vector<DBID> SelectImageIDsWithoutSignature(Lock& guard);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDsWithoutSignature);

    //! \brief Loads a TagCollection for the specified image.
    //! \returns Null if the image is not in the database
    std::shared_ptr<TagCollection> LoadImageTags(const std::shared_ptr<Image>& image);

    //! \brief Retrieves all tags added to an image
    void SelectImageTags(Lock& guard, std::weak_ptr<Image> image,
        std::vector<std::shared_ptr<AppliedTag>>& tags);

    //! \brief Adds a tag to an image
    void InsertImageTag(Lock& guard, std::weak_ptr<Image> image, AppliedTag& tag);

    //! \brief Removes a tag from an image
    void DeleteImageTag(Lock& guard, std::weak_ptr<Image> image, AppliedTag& tag);

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

    //! \brief Inserts a new collection to the database
    //! \exception InvalidSQL if the collection violates unique constraints (same name)
    std::shared_ptr<Collection> InsertCollection(
        Lock& guard, const std::string& name, bool isprivate);
    CREATE_NON_LOCKING_WRAPPER(InsertCollection);

    //! \brief Updates an images properties in the database
    //! \returns False if the update fails
    bool UpdateCollection(const Collection& collection);

    //! \brief Deletes a collection from the database
    //!
    //! The collection object's id will be set to -1
    //! \returns True if succeeded
    bool DeleteCollection(Collection& collection);

    //! \brief Retrieves a Collection based on the name
    std::shared_ptr<Collection> SelectCollectionByName(Lock& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionByName);

    std::vector<std::string> SelectCollectionNamesByWildcard(
        const std::string& pattern, int64_t max = 50);

    //! \brief Retrieves a Collection based on the id
    std::shared_ptr<Collection> SelectCollectionByID(DBID id);

    //! \brief Returns the largest value used for an image in the collection
    //! If the collection is empty returns 0
    int64_t SelectCollectionLargestShowOrder(Lock& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionLargestShowOrder);

    //! \brief Returns the number of images in a collection
    int64_t SelectCollectionImageCount(Lock& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionImageCount);

    //! \brief Loads a TagCollection for the specified collection.
    //! \returns Null if the collection is not in the database
    std::shared_ptr<TagCollection> LoadCollectionTags(
        const std::shared_ptr<Collection>& collection);

    //! \brief Retrieves all tags added to an collection
    void SelectCollectionTags(Lock& guard, std::weak_ptr<Collection> collection,
        std::vector<std::shared_ptr<AppliedTag>>& tags);

    //! \brief Adds a tag to an collection
    void InsertCollectionTag(
        Lock& guard, std::weak_ptr<Collection> collection, AppliedTag& tag);

    //! \brief Removes a tag from an collection
    void DeleteCollectionTag(
        Lock& guard, std::weak_ptr<Collection> collection, AppliedTag& tag);

    //
    // Collection image
    //

    //! \brief Adds image to collection
    //! \returns True if succeeded. False if no rows where changed
    //! \param showorder The order number of the image in the collection. If the same one is
    //! already in use the order in which the two images that have the same show_order is
    //! random
    bool InsertImageToCollection(
        Lock& guard, Collection& collection, Image& image, int64_t showorder);
    CREATE_NON_LOCKING_WRAPPER(InsertImageToCollection);

    //! \brief Returns true if image is in some collection.
    //!
    //! If false it should be added to one otherwise it cannot be viewed
    bool SelectIsImageInAnyCollection(Lock& guard, const Image& image);

    //! \brief Removes an image from the collection
    //! \returns True if removed. False if the image wasn't in the collection
    bool DeleteImageFromCollection(Lock& guard, Collection& collection, Image& image);

    //! \brief Returns the show_order image has in collection. Or -1
    int64_t SelectImageShowOrderInCollection(
        Lock& guard, const Collection& collection, const Image& image);
    CREATE_NON_LOCKING_WRAPPER(SelectImageShowOrderInCollection);

    //! \brief Returns the image with the show order
    //! \note If for some reason there are multiples with the same show_order one of them
    //! is returned
    std::shared_ptr<Image> SelectImageInCollectionByShowOrder(
        Lock& guard, const Collection& collection, int64_t showorder);
    CREATE_NON_LOCKING_WRAPPER(SelectImageInCollectionByShowOrder);

    //! \brief Returns the preview image for a collection
    std::shared_ptr<Image> SelectCollectionPreviewImage(const Collection& collection);

    //! \brief Returns the first image in a collection, or null if empty
    std::shared_ptr<Image> SelectFirstImageInCollection(
        Lock& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectFirstImageInCollection);

    //! \brief Returns the last image in a collection, or null if empty
    std::shared_ptr<Image> SelectLastImageInCollection(
        Lock& guard, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(SelectLastImageInCollection);

    //! \brief Counts the index image has in a collection based on the show_orders
    int64_t SelectImageShowIndexInCollection(const Collection& collection, const Image& image);

    //! \brief Returns the image with the show_order index
    //! \note If for some reason there are multiples with the same show_order one of them
    //! is returned
    std::shared_ptr<Image> SelectImageInCollectionByShowIndex(
        Lock& guard, const Collection& collection, int64_t index);
    CREATE_NON_LOCKING_WRAPPER(SelectImageInCollectionByShowIndex);

    //! \brief Selects the image that has the next show order
    std::shared_ptr<Image> SelectNextImageInCollectionByShowOrder(
        const Collection& collection, int64_t showorder);

    //! \brief Selects the image that has the previous show order
    std::shared_ptr<Image> SelectPreviousImageInCollectionByShowOrder(
        const Collection& collection, int64_t showorder);

    //! \brief Returns all images in a collection
    std::vector<std::shared_ptr<Image>> SelectImagesInCollection(const Collection& collection);

    //
    // Folder
    //
    //! \todo Make this cache the result after the first call to improve performance
    std::shared_ptr<Folder> SelectRootFolder(Lock& guard);
    CREATE_NON_LOCKING_WRAPPER(SelectRootFolder);

    //! \brief Returns a folder by id
    std::shared_ptr<Folder> SelectFolderByID(Lock& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectFolderByID);

    //! \brief Creates a new folder, must have a parent folder
    //! \returns The created folder or null if the name conflicts
    //! \todo Make sure that if there are any quotes they are balanced
    std::shared_ptr<Folder> InsertFolder(
        std::string name, bool isprivate, const Folder& parent);

    bool UpdateFolder(Folder& folder);

    //
    // Folder collection
    //
    bool InsertCollectionToFolder(Lock& guard, Folder& folder, const Collection& collection);
    CREATE_NON_LOCKING_WRAPPER(InsertCollectionToFolder);

    void DeleteCollectionFromFolder(Folder& folder, const Collection& collection);

    //! \brief Returns Collections that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Collection>> SelectCollectionsInFolder(
        const Folder& folder, const std::string& matchingpattern = "");

    //! \brief Returns true if collection is in a Folder
    bool SelectCollectionIsInFolder(Lock& guard, const Collection& collection);

    //! \brief Returns true if Collection is in another folder than folder
    bool SelectCollectionIsInAnotherFolder(
        Lock& guard, const Folder& folder, const Collection& collection);

    //! \brief Returs Folders Collection is in
    std::vector<DBID> SelectFoldersCollectionIsIn(const Collection& collection);

    //! \brief Deletes a Collection from the root folder if the collection is in another
    //! folder.
    void DeleteCollectionFromRootIfInAnotherFolder(const Collection& collection);

    //! \brief Adds a Collection to root if it isn't in any folder
    void InsertCollectionToRootIfInNone(const Collection& collection);

    //
    // Folder folder
    //

    //! \brief Adds a folder to folder
    //! \exception InvalidSQL If fails
    //! \note This doesn't check for name conflicts
    void InsertFolderToFolder(Lock& guard, Folder& folder, const Folder& parent);

    //! \brief Returns Folders that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Folder>> SelectFoldersInFolder(
        const Folder& folder, const std::string& matchingpattern = "");

    //! \brief Selects a folder based on parent folder and name
    std::shared_ptr<Folder> SelectFolderByNameAndParent(
        Lock& guard, const std::string& name, const Folder& parent);
    CREATE_NON_LOCKING_WRAPPER(SelectFolderByNameAndParent);

    //! \brief Selects all parents of a Folder
    std::vector<DBID> SelectFolderParents(const Folder& folder);

    //
    // Tag
    //

    //! \brief Creates a new tag
    std::shared_ptr<Tag> InsertTag(
        std::string name, std::string description, TAG_CATEGORY category, bool isprivate);

    //! \brief Returns tag based on id
    std::shared_ptr<Tag> SelectTagByID(Lock& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByID);

    std::shared_ptr<Tag> SelectTagByName(Lock& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByName);

    //! \brief Selects tags containing pattern
    std::vector<std::shared_ptr<Tag>> SelectTagsWildcard(
        const std::string& pattern, int64_t max = 50, bool aliases = true);

    //! \brief Selects a tag based on an alias name
    std::shared_ptr<Tag> SelectTagByAlias(Lock& guard, const std::string& alias);
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
    std::vector<DBID> SelectTagImplies(Lock& guard, const Tag& tag);


    //
    // AppliedTag
    //
    std::shared_ptr<AppliedTag> SelectAppliedTagByID(Lock& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectAppliedTagByID);

    //! \brief If the tag is in the database (same main tag and other properties)
    //! returns that
    //!
    //! Used when inserting to prefer using existing tags instead of creating a new AppliedTag
    //! for every image and every tag combination
    std::shared_ptr<AppliedTag> SelectExistingAppliedTag(Lock& guard, const AppliedTag& tag);
    CREATE_NON_LOCKING_WRAPPER(SelectExistingAppliedTag);

    //! \brief Returns an existing tag with the same properties as tag
    //! \returns -1 when tag doesn't exist, id on success
    DBID SelectExistingAppliedTagID(Lock& guard, const AppliedTag& tag);
    CREATE_NON_LOCKING_WRAPPER(SelectExistingAppliedTagID);

    std::vector<std::shared_ptr<TagModifier>> SelectAppliedTagModifiers(
        Lock& guard, const AppliedTag& appliedtag);

    //! \brief Returns the right side of a tag combine, appliedtag is the left side
    std::tuple<std::string, std::shared_ptr<AppliedTag>> SelectAppliedTagCombine(
        Lock& guard, const AppliedTag& appliedtag);

    //! \brief Inserts a new AppliedTag to the database
    //! \todo Break this into smaller functions
    bool InsertAppliedTag(Lock& guard, AppliedTag& tag);

    //! \brief Deletes an AppliedTag if it isn't used anymore
    //! \todo Check should this only be called when cleaning up the databse
    //! as there probably won't be infinitely many tags that aren't used
    void DeleteAppliedTagIfNotUsed(Lock& guard, AppliedTag& tag);

    //! \brief Returns true if applied tag id is referenced somewhere
    bool SelectIsAppliedTagUsed(Lock& guard, DBID id);

    //! \brief Checks that the TagModifiers set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagModifiersMatch(Lock& guard, DBID id, const AppliedTag& tag);

    //! \brief Checks that the tag combines set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagCombinesMatch(Lock& guard, DBID id, const AppliedTag& tag);

    //! \brief Helper for looping all applied_tag
    //! \returns -1 on error
    DBID SelectAppliedTagIDByOffset(Lock& guard, int64_t offset);

    //! \brief Combines two applied tags into one
    //!
    //! Used after checking that to applied_tag entries match to remove the duplicate.
    //! First is the ID that will be preserved and all references to second will be changed
    //! to first
    void CombineAppliedTagDuplicate(Lock& guard, DBID first, DBID second);



    //
    // TagModifier
    //
    std::shared_ptr<TagModifier> SelectTagModifierByID(Lock& guard, DBID id);

    std::shared_ptr<TagModifier> SelectTagModifierByName(Lock& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByName);

    std::shared_ptr<TagModifier> SelectTagModifierByAlias(
        Lock& guard, const std::string& alias);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByAlias);

    std::shared_ptr<TagModifier> SelectTagModifierByNameOrAlias(
        Lock& guard, const std::string& name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByNameOrAlias);


    void UpdateTagModifier(const TagModifier& modifier);

    //
    // TagBreakRule
    //
    //! \brief Returns a break rule that can handle str (or might be able to)
    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByStr(const std::string& str);

    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByExactPattern(
        Lock& guard, const std::string& pattern);

    std::vector<std::shared_ptr<TagModifier>> SelectModifiersForBreakRule(
        Lock& guard, const TagBreakRule& rule);

    //! \todo Implement
    void UpdateTagBreakRule(const TagBreakRule& rule);

    //
    // NetGallery
    //

    //! \brief Returns all the netgalleries
    //! \param nodownloaded If true won't include galleries that are marked as downloaded
    std::vector<DBID> SelectNetGalleryIDs(bool nodownloaded);

    //! \brief Returns a NetGallery by id
    std::shared_ptr<NetGallery> SelectNetGalleryByID(Lock& guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectNetGalleryByID);

    //! \brief Adds a new NetGallery to the database
    //! \returns True if added, false if it is already added
    bool InsertNetGallery(Lock& guard, std::shared_ptr<NetGallery> gallery);

    void UpdateNetGallery(NetGallery& gallery);

    //
    // NetFile
    //

    //! \brief Returns all the netgalleries
    //! \param nodownloaded If true won't include galleries that are marked as downloaded
    std::vector<std::shared_ptr<NetFile>> SelectNetFilesFromGallery(NetGallery& gallery);

    //! \brief Returns a NetFile by id
    std::shared_ptr<NetFile> SelectNetFileByID(Lock& guard, DBID id);

    //! \brief Adds a new NetFile to the database
    void InsertNetFile(Lock& guard, NetFile& netfile, NetGallery& gallery);

    void UpdateNetFile(NetFile& netfile);

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
    // Database maintainance functions
    //
    //! \brief Finds all AppliedTags that have the same properties and combines them
    void CombineAllPossibleAppliedTags(Lock& guard);

    //! \brief Returns the number of rows in applied_tag
    int64_t CountAppliedTags();

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
    void BeginTransaction(Lock& guardLocked, bool alsoauxiliary = false);

    void CommitTransaction(Lock& guardLocked, bool alsoauxiliary = false);

    //! \brief Alternative to transaction that can be nested
    //!
    //! Must be accompanied by a ReleaseSavePoint call
    //! \warning The savepoint name is not sent as a prepared statement, SQL injection is
    //! possible!
    void BeginSavePoint(
        Lock& guard, const std::string& savepointname, bool alsoauxiliary = false);

    void ReleaseSavePoint(
        Lock& guard, const std::string& savepointname, bool alsoauxiliary = false);

    //! \brief Returns true if a transaction is in progress
    bool HasActiveTransaction(Lock& guard);

    // Statistics functions //
    size_t CountExistingTags();


protected:
    //! \brief Runs a command and prints all the result rows with column headers to log
    void PrintResultingRows(Lock& guard, sqlite3* db, const std::string& str);

    //! \brief Runs a command and prints all the result rows with column headers to log
    //!
    //! This version allows settings parameters
    template<typename... TBindTypes>
    void PrintResultingRows(
        Lock& guard, sqlite3* db, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(db, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        LOG_INFO("SQL result from: \"" + str + "\"");
        statementobj.StepAndPrettyPrint(statementinuse);
    }

    //! \brief Runs SQL statement as a prepared statement with the values
    template<typename... TBindTypes>
    void RunSQLAsPrepared(Lock& guard, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(SQLiteDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        statementobj.StepAll(statementinuse);
    }


    //! \brief Runs SQL statement as a prepared statement with the values on the signature DB
    template<typename... TBindTypes>
    void RunOnSignatureDB(Lock& guard, const std::string& str, TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(PictureSignatureDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        statementobj.StepAll(statementinuse);
    }

    //! \brief Runs a raw sql query.
    //! \note Don't use unless absolutely necessary prefer to use Database::RunSqlAsPrepared
    void _RunSQL(Lock& guard, const std::string& sql);

    //! \brief Variant for auxiliary dbs
    void _RunSQL(Lock& guard, sqlite3* db, const std::string& sql);

    //! \brief Throws an InvalidSQL exception, filling it with values from the database
    //! connection
    void ThrowCurrentSqlError(Lock& guard);

private:
    //
    // Row parsing functions
    //

    std::shared_ptr<NetFile> _LoadNetFileFromRow(Lock& guard, PreparedStatement& statement);

    std::shared_ptr<NetGallery> _LoadNetGalleryFromRow(
        Lock& guard, PreparedStatement& statement);

    //! \brief Loads a TagBreakRule object from the current row
    std::shared_ptr<TagBreakRule> _LoadTagBreakRuleFromRow(
        Lock& guard, PreparedStatement& statement);

    //! \brief Loads an AppliedTag object from the current row
    std::shared_ptr<AppliedTag> _LoadAppliedTagFromRow(
        Lock& guard, PreparedStatement& statement);

    //! \brief Loads a TagModifier object from the current row
    std::shared_ptr<TagModifier> _LoadTagModifierFromRow(
        Lock& guard, PreparedStatement& statement);

    //! \brief Loads a Tag object from the current row
    std::shared_ptr<Tag> _LoadTagFromRow(Lock& guard, PreparedStatement& statement);

    //! \brief Loads a Collection object from the current row
    std::shared_ptr<Collection> _LoadCollectionFromRow(
        Lock& guard, PreparedStatement& statement);

    //! \brief Loads an Image object from the current row
    std::shared_ptr<Image> _LoadImageFromRow(Lock& guard, PreparedStatement& statement);

    //! \brief Loads a Folder object from the current row
    std::shared_ptr<Folder> _LoadFolderFromRow(Lock& guard, PreparedStatement& statement);

    //
    // Private insert stuff
    //
    void InsertTagImage(Lock& guard, Image& image, DBID appliedtagid);

    void InsertTagCollection(Lock& guard, Collection& image, DBID appliedtagid);

    //! \brief Inserts the image signature to the auxiliary DB
    void _InsertImageSignatureParts(Lock& guard, DBID image, const std::string& signature);

    //
    // Utility stuff
    //

    //! \brief Creates default tables and also calls _InsertDefaultTags
    void _CreateTableStructure(Lock& guard);

    //! \brief Variant for the picture signature auxiliary db
    void _CreateTableStructureSignatures(Lock& guard);

    //! \brief Verifies that the specified version is compatible with the current version
    bool _VerifyLoadedVersion(Lock& guard, int fileversion);

    //! \brief Variant for the picture signature auxiliary db
    bool _VerifyLoadedVersionSignatures(Lock& guard, int fileversion);

    //! \brief Called if a loaded database version is older than DATABASE_CURRENT_VERSION
    //! \param oldversion The version from which the update is done, will contain the new
    //! version after this returns. Can be called again to update to the next version
    //! \returns True if succeeded, false if update is not supported
    bool _UpdateDatabase(Lock& guard, const int oldversion);

    //! \brief Variant for the picture signature auxiliary db
    bool _UpdateDatabaseSignatures(Lock& guard, const int oldversion);

    //! \brief Sets the database version. Should only be called from _UpdateDatabase
    void _SetCurrentDatabaseVersion(Lock& guard, int newversion);

    //! \brief Variant for the picture signature auxiliary db
    void _SetCurrentDatabaseVersionSignatures(Lock& guard, int newversion);

    //! \brief Helper for updates. Might be useful integrity checks later
    //! \note This can only be ran while doing a database update because the database
    //! needs to parse tags and to do that it needs to be unlocked and it might mess with
    //! the result that is being iterated
    void _UpdateApplyDownloadTagStrings(Lock& guard);

protected:
    sqlite3* SQLiteDb = nullptr;

    //! This has the image signature table
    sqlite3* PictureSignatureDb = nullptr;

    //! Used for backups before potentially dangerous operations
    std::string DatabaseFile;

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
};

//! \brief Helper class that automatically commits a transaction when it destructs
//!\ todo Would be nice to be able to conditionally create these
class DoDBTransaction {
public:
    DoDBTransaction(Database& db, Lock& dblock, bool alsoauxiliary = false);

    DoDBTransaction(DoDBTransaction&& other) = delete;
    DoDBTransaction(const DoDBTransaction& other) = delete;
    DoDBTransaction& operator=(const DoDBTransaction& other) = delete;

    ~DoDBTransaction();

private:
    Database& DB;
    Lock& Locked;
    bool Auxiliary;
};

//! \brief Helper class that automatically handles a savepoint
//!\ see DoDBTransaction
//! \warning The savepoint name is not sent as a prepared statement, SQL injection is possible!
class DoDBSavePoint {
public:
    DoDBSavePoint(
        Database& db, Lock& dblock, const std::string& savepoint, bool alsoauxiliary = false);

    DoDBSavePoint(DoDBSavePoint&& other) = delete;
    DoDBSavePoint(const DoDBSavePoint& other) = delete;
    DoDBSavePoint& operator=(const DoDBSavePoint& other) = delete;

    ~DoDBSavePoint();

private:
    Database& DB;
    Lock& Locked;
    const std::string SavePoint;
    bool Auxiliary;
};

} // namespace DV
