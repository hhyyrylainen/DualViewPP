#pragma once

#include "leviathan/Common/BaseNotifier.h"

#include <cstdint>


namespace DV{

class Database;

//! \brief Base class for all resources that can be saved and loaded from the database
class DatabaseResource : public Leviathan::BaseNotifierAll{
    friend Database;
public:

    //! \brief Constructor for objects that are created in order to save them later
    //! to the database
    DatabaseResource(bool notloadedfromdb);

    //! \brief Database calls this constructor (indirectly) when loading
    DatabaseResource(int64_t id, Database &from);
    
    virtual ~DatabaseResource();

    //! \brief Saves this object to the database if it is marked dirty
    void Save();

    //! \brief Called when this gets marked dirty, used to notify listeners to update
    //! their cached values
    void OnMarkDirty(){

        IsDirty = true;
        NotifyAll();
    }

    inline auto GetID() const{

        return ID;
    }

    //! \brief Returns true if this is in the database
    bool IsInDatabase() const{

        return InDatabase && ID != -1;
    }

    //! \brief Returns true if the IDs match and they aren't -1
    bool operator ==(const DatabaseResource &other) const{

        return (ID == other.ID && ID != -1 && InDatabase);
    }

protected:

    virtual void _DoSave(Database &db) = 0;
    
protected:
    
    int64_t ID = -1;
    bool IsDirty = false;

    //! True if loaded from the database. This could also be a pointer that would either be
    //! null or a pointer to the Database instance
    bool InDatabase = false;
};


}
