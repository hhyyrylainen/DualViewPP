#pragma once

#include "DatabaseResource.h"
#include "Tags.h"

#include "leviathan/Common/Types.h"

#include <string>
#include <chrono>
#include <memory>

namespace DV{

class Image;
class DatabaseTagCollection;

class Database;
class PreparedStatement;

class Collection : public DatabaseResource{
public:
    //! \brief Creates a collection for database testing
    //! \protected
    Collection(const std::string &name);

public:

    //! \brief Database load function
    Collection(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);

    //! \brief Return Name with illegal characters replaced with spaces
    std::string GetNameForFolder() const;

    //! \brief Adds tags to this collection
    //! \note Only works if this is in the database
    //! \return True if successfully added
    bool AddTags(const TagCollection &tags);


    //! \brief Adds an image to this Collection
    //!
    //! The images show_order will be set to be the last image in the collection
    bool AddImage(std::shared_ptr<Image> image);

    //! \brief Adds an image to this Collection
    //!
    //! The images show_order will be set to be the last image in the collection
    bool AddImage(std::shared_ptr<Image> image, int64_t order);

    //! \brief Removes an image from this collection
    //! \returns True if the image was removed, false if it wasn't in this collection
    bool RemoveImage(std::shared_ptr<Image> image);

    //! \brief Gets the largest show_order used in the collection
    int64_t GetLastShowOrder();

    inline auto GetIsPrivate() const{

        return IsPrivate;
    }

    inline auto GetName() const{

        return Name;
    }
    

protected:

    // DatabaseResource implementation
    void _DoSave(Database &db) override;

protected:

    std::string Name;

    std::chrono::system_clock::time_point AddDate = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point ModifyDate = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point LastView = std::chrono::system_clock::now();

    bool IsPrivate = false;

    //! If set has the highest show_order of any image in the collection
    int64_t LastOrder;
    //! If true LastOrder is valid
    bool LastOrderSet = false;


    std::shared_ptr<DatabaseTagCollection> Tags;
};

}
