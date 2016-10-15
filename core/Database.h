#pragma once

#include "Common/ThreadSafe.h"
#include "SQLHelpers.h"

#include "SingleLoad.h"

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

//! \brief The version number of the database
constexpr auto DATABASE_CURRENT_VERSION = 14;

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

    //! \brief Retrieves an Image's id based on the hash
    DBID SelectImageIDByHash(Lock &guard, const std::string &hash);
    CREATE_NON_LOCKING_WRAPPER(SelectImageIDByHash);
    
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

    // Statistics functions //
    size_t CountExistingTags();

    //! \brief Helper callback for standard operations
    //! \warning The user parameter has to be a pointer to Database::GrabResultHolder
    static int SqliteExecGrabResult(void* user, int columns, char** columnsastext,
        char** columnname);

    //
    // Folder
    //
    std::shared_ptr<Folder> SelectRootFolder(Lock &guard);
    CREATE_NON_LOCKING_WRAPPER(SelectRootFolder);

    bool UpdateFolder(Folder &folder);

    //
    // Folder collection
    //
    bool InsertCollectionToFolder(Lock &guard, Folder &folder, Collection &collection);
    
    
private:

    //
    // Row parsing functions
    //
    
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
    bool _UpdateDatabase(Lock &guard, int &oldversion);

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
};

}

