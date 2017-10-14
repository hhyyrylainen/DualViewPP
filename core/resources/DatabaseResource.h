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
        GUARD_LOCK();
        NotifyAll(guard);
    }

    inline auto GetID() const{

        return ID;
    }

    //! \brief Returns true if this is in the database
    inline bool IsInDatabase() const{

        return InDatabase && ID != -1;
    }

    //! \brief Returns true if the IDs match and they aren't -1
    bool operator ==(const DatabaseResource &other) const{

        return (ID == other.ID && ID != -1 && InDatabase);
    }

    //! \brief Not ==
    bool operator !=(const DatabaseResource &other) const{

        return !(*this == other);
    }

protected:

    //! When a resource becomes a duplicate of another
    void _BecomeDuplicateOf(const DatabaseResource &other);

    //! When database has added this as a resource, this is called
    void OnAdopted(int64_t id, Database &from);

    virtual void _DoSave(Database &db) = 0;

    //! \brief Callback for child classes to reload resources when added to the database
    virtual void _OnAdopted();
    
protected:
    
    int64_t ID = -1;
    bool IsDirty = false;

    //! Points to the database this was loaded from, or null
    Database* InDatabase = nullptr;

    //! \brief Must be called by all classes inheriting from this, in their destructors
    void DBResourceDestruct();

private:

    //! Set to true once DBResourceDestruct has been called
    bool _DestructCalled = false;
};


}
