// ------------------------------------ //
#include "ChangeEvents.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
    

ChangeEvents::ChangeEvents(){

    for(uint32_t i = 0; i < static_cast<uint32_t>(CHANGED_EVENT::MAX); ++i){

        RegisteredEvents[i] = std::make_shared<EventSlot>(static_cast<CHANGED_EVENT>(i));
    }
}

ChangeEvents::~ChangeEvents(){

    
}
// ------------------------------------ //
void ChangeEvents::RegisterForEvent(CHANGED_EVENT event, Leviathan::BaseNotifiableAll* object,
    Lock &objectlock)
{
    LEVIATHAN_ASSERT(event >= static_cast<CHANGED_EVENT>(0) && event < CHANGED_EVENT::MAX,
        "Invalid event number in ChagneEvents");

    const auto& slot = RegisteredEvents[static_cast<size_t>(event)];

    GUARD_LOCK_OTHER(*slot);
    slot->ConnectToNotifiable(guard, object, objectlock);
}

void ChangeEvents::FireEvent(CHANGED_EVENT event) const{

    LEVIATHAN_ASSERT(event >= static_cast<CHANGED_EVENT>(0) && event < CHANGED_EVENT::MAX,
        "Invalid event number in ChagneEvents");

    const auto& slot = RegisteredEvents[static_cast<size_t>(event)];
    slot->NotifyAll();
}
