#pragma once


#include "leviathan/Common/BaseNotifier.h"


namespace DV{

enum class CHANGED_EVENT : uint32_t {

    //! Fired when a DownloadGallery is inserted to the database
    DOWNLOAD_GALLERY_CREATED = 0,

    //! Fired when a new Collection is inserted
    COLLECTION_CREATED,
    
    MAX
};

//! \brief This is the actual object that ChangeEvents attaches listeners to
class EventSlot : public Leviathan::BaseNotifierAll{
public:
    inline EventSlot(CHANGED_EVENT event) : EventsType(event){};
    
    const CHANGED_EVENT EventsType;
};

//! \brief Manager class for resource update events originating from the database
//!
//! Object specific changes are dispatched from the objects themselves, but global change
//! events that are initiated through the database (and not changing an object's property)
//! go through this object
class ChangeEvents{    
public:

    //! fills RegisteredEvents with empty slots
    ChangeEvents();
    ~ChangeEvents();
    
    
    //! \brief Registers for an event
    //! \note This will check for duplicates and skip adding duplicates
    //! \warning Trying to call with an invalid event will assert
    void RegisterForEvent(CHANGED_EVENT event, Leviathan::BaseNotifiableAll* object,
        Lock &objectlock);

    //! \brief Fires an event
    //!
    //! Should only be called by the Database or any other code that manages the resources
    //! for which the events are meant.
    //! \note Trying to fire the same event recursively may deadlock
    //! \warning Trying to call an invalid event will assert
    void FireEvent(CHANGED_EVENT event) const;
    
    
private:

    //! Contains a listener slot for each event type
    std::shared_ptr<EventSlot> RegisteredEvents[static_cast<uint32_t>(CHANGED_EVENT::MAX)];
};

}
