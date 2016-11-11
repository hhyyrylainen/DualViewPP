#pragma once

#include "Common/ThreadSafe.h"
#include "SQLHelpers.h"

#include "SingleLoad.h"
#include "PreparedStatement.h"

#include "Common.h"

#include <string>
#include <vector>

#include <thread>

// Forward declare sqlite //
struct sqlite3;

namespace DV{

class PreparedStatement;

class CurlWrapper;

class Image;
class Collection;
class Folder;
class TagCollection;

class Tag;
class AppliedTag;
class TagModifier;
class TagBreakRule;

//! \brief The version number of the database
constexpr auto DATABASE_CURRENT_VERSION = 16;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
class Database : public Leviathan::ThreadSafe{

    //! \brief Holds data in Database::SqliteExecGrabResult
    struct GrabResultHolder{

        struct Row{

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
    //! \returns True if succeeded, false if np version exists.
    bool SelectDatabaseVersion(Lock &guard, int &result);

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
    void InsertImage(Image &image);

    //! \brief Updates an images properties in the database
    //! \returns False if the update fails
    bool UpdateImage(const Image &image);

    //! \brief Deletes an image from the database
    //!
    //! The image object's id will be set to -1
    //! \returns True if succeeded
    bool DeleteImage(Image &image);

    //! \brief Retrieves an Image based on the hash
    std::shared_ptr<Image> SelectImageByHash(Lock &guard, const std::string &hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByHash);

    //! \brief Retrieves an Image based on the id
    std::shared_ptr<Image> SelectImageByID(Lock &guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectImageByID);

    //! \brief Retrieves an Image's id based on the hash
    DBID SelectImageIDByHash(Lock &guard, const std::string &hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDByHash);

    //! \brief Loads a TagCollection for the specified image.
    //! \returns Null if the image is not in the database
    std::shared_ptr<TagCollection> LoadImageTags(const std::shared_ptr<Image> &image);

    //! \brief Retrieves all tags added to an image
    void SelectImageTags(std::weak_ptr<Image> image,
        std::vector<std::shared_ptr<AppliedTag>> &tags);

    //! \brief Adds a tag to an image
    void InsertImageTag(std::weak_ptr<Image> image,
        AppliedTag &tag);

    //! \brief Removes a tag from an image
    void DeleteImageTag(std::weak_ptr<Image> image,
        AppliedTag &tag);

    //
    // Collection functions
    //

    //! \brief Inserts a new collection to the database
    //! \exception InvalidSQL if the collection violates unique constraints (same name)
    std::shared_ptr<Collection> InsertCollection(Lock &guard,
        const std::string &name, bool isprivate);
    CREATE_NON_LOCKING_WRAPPER(InsertCollection);

    //! \brief Updates an images properties in the database
    //! \returns False if the update fails
    bool UpdateCollection(const Collection &collection);

    //! \brief Deletes a collection from the database
    //!
    //! The collection object's id will be set to -1
    //! \returns True if succeeded
    bool DeleteCollection(Collection &collection);

    //! \brief Retrieves a Collection based on the name
    std::shared_ptr<Collection> SelectCollectionByName(Lock &guard, const std::string &name);
    CREATE_NON_LOCKING_WRAPPER(SelectCollectionByName);
    
    //! \brief Retrieves a Collection based on the id
    std::shared_ptr<Collection> SelectCollectionByID(DBID id);

    //! \brief Returns the largest value used for an image in the collection
    //! If the collection is empty returns 0
    int64_t SelectCollectionLargestShowOrder(const Collection &collection);

    //! \brief Returns the number of images in a collection
    int64_t SelectCollectionImageCount(const Collection &collection);

    //! \brief Loads a TagCollection for the specified collection.
    //! \returns Null if the collection is not in the database
    std::shared_ptr<TagCollection> LoadCollectionTags(
        const std::shared_ptr<Collection> &collection);

    //! \brief Retrieves all tags added to an collection
    void SelectCollectionTags(std::weak_ptr<Collection> collection,
        std::vector<std::shared_ptr<AppliedTag>> &tags);

    //! \brief Adds a tag to an collection
    void InsertCollectionTag(std::weak_ptr<Collection> collection,
        AppliedTag &tag);

    //! \brief Removes a tag from an collection
    void DeleteCollectionTag(std::weak_ptr<Collection> collection,
        AppliedTag &tag);

    //
    // Collection image 
    //

    //! \brief Adds image to collection
    //! \returns True if succeeded. False if no rows where changed
    //! \param showorder The order number of the image in the collection. If the same one is
    //! already in use the order in which the two images that have the same show_order is
    //! random
    bool InsertImageToCollection(Collection &collection, Image &image, int64_t showorder);

    //! \brief Removes an image from the collection
    //! \returns True if removed. False if the image wasn't in the collection
    bool DeleteImageFromCollection(Collection &collection, Image &image);

    //! \brief Returns the show_order image has in collection. Or -1
    int64_t SelectImageShowOrderInCollection(Collection &collection, Image &image);

    //! \brief Returns the preview image for a collection
    std::shared_ptr<Image> SelectCollectionPreviewImage(const Collection &collection);

    //! \brief Returns the first image in a collection, or null if empty
    std::shared_ptr<Image> SelectFirstImageInCollection(Lock &guard,
        const Collection &collection);
    CREATE_NON_LOCKING_WRAPPER(SelectFirstImageInCollection);

    //! \brief Returns all images in a collection
    std::vector<std::shared_ptr<Image>> SelectImagesInCollection(const Collection &collection);

    // Statistics functions //
    size_t CountExistingTags();

    //! \brief Helper callback for standard operations
    //! \warning The user parameter has to be a pointer to Database::GrabResultHolder
    static int SqliteExecGrabResult(void* user, int columns, char** columnsastext,
        char** columnname);

    //
    // Folder
    //
    //! \todo Make this cache the result after the first call to improve performance
    std::shared_ptr<Folder> SelectRootFolder(Lock &guard);
    CREATE_NON_LOCKING_WRAPPER(SelectRootFolder);

    //! \brief Returns a folder by id
    std::shared_ptr<Folder> SelectFolderByID(Lock &guard, DBID id);

    //! \brief Creates a new folder, must have a parent folder
    //! \returns The created folder or null if the name conflicts
    //! \todo Make sure that if there are any quotes they are balanced
    std::shared_ptr<Folder> InsertFolder(std::string name, bool isprivate,
        const Folder &parent);

    bool UpdateFolder(Folder &folder);
    
    //
    // Folder collection
    //
    bool InsertCollectionToFolder(Lock &guard, Folder &folder, Collection &collection);
    CREATE_NON_LOCKING_WRAPPER(InsertCollectionToFolder);

    //! \brief Returns Collections that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Collection>> SelectCollectionsInFolder(const Folder &folder,
        const std::string &matchingpattern = "");

    //! \brief Returns true if Collection is in another folder than folder
    bool SelectCollectionIsInAnotherFolder(Lock &guard, const Folder &folder,
        const Collection &collection);

    //! \brief Deletes a Collection from the root folder if the collection is in another
    //! folder.
    void DeleteCollectionFromRootIfInAnotherFolder(const Collection &collection);

    //
    // Folder folder
    //

    //! \brief Adds a folder to folder
    //! \exception InvalidSQL If fails
    //! \note This doesn't check for name conflicts
    void InsertFolderToFolder(Lock &guard, Folder &folder, const Folder &parent);
    
    //! \brief Returns Folders that are directly in folder. And their name contains
    //! matching pattern
    std::vector<std::shared_ptr<Folder>> SelectFoldersInFolder(const Folder &folder,
        const std::string &matchingpattern = "");

    //! \brief Selects a folder based on parent folder and name
    std::shared_ptr<Folder> SelectFolderByNameAndParent(Lock &guard, const std::string &name,
        const Folder &parent);
    CREATE_NON_LOCKING_WRAPPER(SelectFolderByNameAndParent);


    //
    // Tag
    //

    //! \brief Creates a new tag
    std::shared_ptr<Tag> InsertTag(std::string name, std::string description,
        TAG_CATEGORY category, bool isprivate);
    
    //! \brief Returns tag based on id
    std::shared_ptr<Tag> SelectTagByID(Lock &guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByID);

    std::shared_ptr<Tag> SelectTagByName(Lock &guard, const std::string &name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByName);

    //! \brief Selects tags containing pattern
    std::vector<std::shared_ptr<Tag>> SelectTagsWildcard(const std::string &pattern,
        int max = 50);

    //! \brief Selects a tag based on an alias name
    std::shared_ptr<Tag> SelectTagByAlias(Lock &guard, const std::string &alias);
    CREATE_NON_LOCKING_WRAPPER(SelectTagByAlias);

    //! \brief Selects a tag matching the name or has an alias for the name
    std::shared_ptr<Tag> SelectTagByNameOrAlias(const std::string &name);

    //! \brief Returns the text of a super alias
    std::string SelectTagSuperAlias(const std::string &name);

    //! \brief Updates Tag's properties
    void UpdateTag(Tag &tag);

    //! \brief Adds an alias to a tag
    //! \returns True if added, false if the alias is already in use
    bool InsertTagAlias(Tag &tag, const std::string &alias);

    //! \brief Deletes a tag alias
    void DeleteTagAlias(const std::string &alias);

    //! \brief Deletes a tag alias if it is an alias for tag
    void DeleteTagAlias(const Tag &tag, const std::string &alias);

    //! \brief Returns the tags implied by tag
    std::vector<std::shared_ptr<Tag>> SelectTagImpliesAsTag(const Tag &tag);

    //! \brief Returns the ids of tags implied by tag
    std::vector<DBID> SelectTagImplies(Lock &guard, const Tag &tag);

    
    //
    // AppliedTag
    //
    std::shared_ptr<AppliedTag> SelectAppliedTagByID(Lock &guard, DBID id);
    CREATE_NON_LOCKING_WRAPPER(SelectAppliedTagByID);

    //! \brief If the tag is in the database (same main tag and other properties)
    //! returns that
    //!
    //! Used when inserting to prefer using existing tags instead of creating a new AppliedTag
    //! for every image and every tag combination
    std::shared_ptr<AppliedTag> SelectExistingAppliedTag(Lock &guard, const AppliedTag &tag);

    //! \brief Returns an existing tag with the same properties as tag
    //! \returns -1 when tag doesn't exist, id on success
    DBID SelectExistingAppliedTagID(Lock &guard, const AppliedTag &tag);

    std::vector<std::shared_ptr<TagModifier>> SelectAppliedTagModifiers(Lock &guard,
        const AppliedTag &appliedtag);

    //! \brief Returns the right side of a tag combine, appliedtag is the left side
    std::tuple<std::string, std::shared_ptr<AppliedTag>> SelectAppliedTagCombine(Lock &guard,
        const AppliedTag &appliedtag);

    //! \brief Inserts a new AppliedTag to the database
    //! \todo Break this into smaller functions
    bool InsertAppliedTag(Lock &guard, AppliedTag &tag);

    //! \brief Deletes an AppliedTag if it isn't used anymore
    //! \todo Check should this only be called when cleaning up the databse
    //! as there probably won't be infinitely many tags that aren't used
    void DeleteAppliedTagIfNotUsed(Lock &guard, AppliedTag &tag);

    //! \brief Returns true if applied tag id is referenced somewhere
    bool SelectIsAppliedTagUsed(Lock &guard, DBID id);

    //! \brief Checks that the TagModifiers set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagModifiersMatch(Lock &guard, DBID id, const AppliedTag &tag);

    //! \brief Checks that the tag combines set in the database to id, match the ones in tag
    bool CheckDoesAppliedTagCombinesMatch(Lock &guard, DBID id, const AppliedTag &tag);

    //! \brief Helper for looping all applied_tag
    //! \returns -1 on error
    DBID SelectAppliedTagIDByOffset(Lock &guard, int64_t offset);

    //! \brief Combines two applied tags into one
    //!
    //! Used after checking that to applied_tag entries match to remove the duplicate.
    //! First is the ID that will be preserved and all references to second will be changed
    //! to first
    void CombineAppliedTagDuplicate(Lock &guard, DBID first, DBID second);



    //
    // TagModifier
    //
    std::shared_ptr<TagModifier> SelectTagModifierByID(Lock &guard, DBID id);

    std::shared_ptr<TagModifier> SelectTagModifierByName(Lock &guard, const std::string &name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByName);

    std::shared_ptr<TagModifier> SelectTagModifierByAlias(Lock &guard,
        const std::string &alias);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByAlias);

    std::shared_ptr<TagModifier> SelectTagModifierByNameOrAlias(Lock &guard,
        const std::string &name);
    CREATE_NON_LOCKING_WRAPPER(SelectTagModifierByNameOrAlias);

    
    void UpdateTagModifier(const TagModifier &modifier);

    //
    // TagBreakRule
    //
    //! \brief Returns a break rule that can handle str (or might be able to)
    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByStr(const std::string &str);

    std::shared_ptr<TagBreakRule> SelectTagBreakRuleByExactPattern(Lock &guard,
        const std::string &pattern);
    
    std::vector<std::shared_ptr<TagModifier>> SelectModifiersForBreakRule(Lock &guard,
        const TagBreakRule &rule);

    //! \todo Implement
    void UpdateTagBreakRule(const TagBreakRule &rule);


    //
    // Wilcard searches for tag suggestions
    //
    //! \brief Returns text of all break rules that contain str
    void SelectTagBreakRulesByStrWildcard(std::vector<std::string> &breakrules,
        const std::string &pattern);

    void SelectTagNamesWildcard(std::vector<std::string> &result, const std::string &pattern);
    
    void SelectTagAliasesWildcard(std::vector<std::string> &result,
        const std::string &pattern);

    void SelectTagModifierNamesWildcard(std::vector<std::string> &result,
        const std::string &pattern);
    
    void SelectTagSuperAliasWildcard(std::vector<std::string> &result,
        const std::string &pattern);
    
    //
    // Database maintainance functions
    //
    //! \brief Finds all AppliedTags that have the same properties and combines them
    void CombineAllPossibleAppliedTags(Lock &guard);

    //! \brief Returns the number of rows in applied_tag
    int64_t CountAppliedTags();
    
    //! \brief Tries to escape quotes in a string for insertion to sql statements
    static std::string EscapeSql(std::string str);

protected:

    //! \brief Runs a command and prints all the result rows with column headers to log
    void PrintResultingRows(Lock &guard, const std::string &str);

    //! \brief Runs a command and prints all the result rows with column headers to log
    //!
    //! This version allows settings parameters
    template<typename... TBindTypes>
        void PrintResultingRows(Lock &guard, const std::string &str,
            TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(SQLiteDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...); 

        LOG_INFO("SQL result from: \"" + str + "\"");
        statementobj.StepAndPrettyPrint(statementinuse);
    }

    //! \brief Runs SQL statement as a prepared statement with the values
    template<typename... TBindTypes>
        void RunSQLAsPrepared(Lock &guard, const std::string &str,
            TBindTypes&&... valuestobind)
    {
        PreparedStatement statementobj(SQLiteDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...); 

        statementobj.StepAll(statementinuse);
    }

    //! \brief Runs a raw sql query.
    //! \note Don't use unless absolutely necessary prefer to use Database::RunSqlAsPrepared
    void _RunSQL(Lock &guard, const std::string &sql);
    
private:

    //
    // Row parsing functions
    //

    //! \brief Loads a TagBreakRule object from the current row
    std::shared_ptr<TagBreakRule> _LoadTagBreakRuleFromRow(Lock &guard,
        PreparedStatement &statement);

    //! \brief Loads an AppliedTag object from the current row
    std::shared_ptr<AppliedTag> _LoadAppliedTagFromRow(Lock &guard,
        PreparedStatement &statement);

    //! \brief Loads a TagModifier object from the current row
    std::shared_ptr<TagModifier> _LoadTagModifierFromRow(Lock &guard,
        PreparedStatement &statement);

    //! \brief Loads a Tag object from the current row
    std::shared_ptr<Tag> _LoadTagFromRow(Lock &guard, PreparedStatement &statement);
    
    //! \brief Loads a Collection object from the current row
    std::shared_ptr<Collection> _LoadCollectionFromRow(Lock &guard,
        PreparedStatement &statement);

    //! \brief Loads an Image object from the current row
    std::shared_ptr<Image> _LoadImageFromRow(Lock &guard,
        PreparedStatement &statement);

    //! \brief Loads a Folder object from the current row
    std::shared_ptr<Folder> _LoadFolderFromRow(Lock &guard,
        PreparedStatement &statement);

    //
    // Private insert stuff
    //
    void InsertTagImage(Image &image, DBID appliedtagid);

    void InsertTagCollection(Collection &image, DBID appliedtagid);
    
    //
    // Utility stuff
    //
    
    //! \brief Throws an InvalidSQL exception, filling it with values from the database
    //! connection
    void ThrowCurrentSqlError(Lock &guard);

    //! \brief Creates default tables and also calls _InsertDefaultTags
    void _CreateTableStructure(Lock &guard);
    
    //! \brief Inserts default inbuilt tags
    void _InsertDefaultTags(Lock &guard);

    //! \brief Verifies that the specified version is compatible with the current version
    bool _VerifyLoadedVersion(Lock &guard, int fileversion);

    //! \brief Called if a loaded database version is older than DATABASE_CURRENT_VERSION
    //! \param oldversion The version from which the update is done, will contain the new
    //! version after this returns. Can be called again to update to the next version
    //! \returns True if succeeded, false if update is not supported
    bool _UpdateDatabase(Lock &guard, const int oldversion);

    //! \brief Sets the database version. Should only be called from _UpdateDatabase
    void _SetCurrentDatabaseVersion(Lock &guard, int newversion);
    
protected:


    sqlite3* SQLiteDb = nullptr;

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

};

}

