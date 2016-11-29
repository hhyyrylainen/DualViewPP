#include "catch.hpp"

#include "core/ChangeEvents.h"

using namespace DV;

class NotifyChecker : public Leviathan::BaseNotifiableAll{
public:

    void OnNotified(Lock &ownlock, Leviathan::BaseNotifierAll* parent, Lock &parentlock)
        override
    {
        Notified = true;
    }
    
    bool Notified = false;
};

TEST_CASE("Change events work correctly", "[db][events]"){

    ChangeEvents events;

    NotifyChecker obj1;
    NotifyChecker obj2;
    NotifyChecker obj3;


    SECTION("Single event fire"){

        {
            GUARD_LOCK_OTHER(obj1);
            events.RegisterForEvent(CHANGED_EVENT::NET_GALLERY_CREATED, &obj1, guard);
        }

        CHECK(!obj1.Notified);

        events.FireEvent(CHANGED_EVENT::NET_GALLERY_CREATED);

        CHECK(obj1.Notified);
        CHECK(!obj2.Notified);
        CHECK(!obj3.Notified);
    }

    SECTION("Single event doesn't fire other events"){

        {
            GUARD_LOCK_OTHER(obj1);
            events.RegisterForEvent(CHANGED_EVENT::NET_GALLERY_CREATED, &obj1, guard);
        }

        {
            GUARD_LOCK_OTHER(obj2);
            events.RegisterForEvent(CHANGED_EVENT::COLLECTION_CREATED, &obj2, guard);
        }

        {
            GUARD_LOCK_OTHER(obj3);
            events.RegisterForEvent(CHANGED_EVENT::NET_GALLERY_CREATED, &obj3, guard);
        }

        events.FireEvent(CHANGED_EVENT::NET_GALLERY_CREATED);

        CHECK(obj1.Notified);
        CHECK(!obj2.Notified);
        CHECK(obj3.Notified);

        events.FireEvent(CHANGED_EVENT::COLLECTION_CREATED);
        
        CHECK(obj1.Notified);
        CHECK(obj2.Notified);
        CHECK(obj3.Notified);

        obj1.Notified = false;
        obj2.Notified = false;
        obj3.Notified = false;

        events.FireEvent(CHANGED_EVENT::COLLECTION_CREATED);

        CHECK(!obj1.Notified);
        CHECK(obj2.Notified);
        CHECK(!obj3.Notified);
    }


}
