#pragma once

#include <memory>

namespace DV{

//! \brief Helper class to make sure database functions don't use deleted objects
//! when invoking back
//! \note This is not locked because it is unneeded. Because GTK objects will be only destroyed
//! on the main thread and if an Invoke is currently running on the main thread the object
//! won't be getting destroyed while it is running
class IsAlive{
public:

    using AliveMarkerT = std::weak_ptr<bool>;

    static bool IsStillAlive(const AliveMarkerT &objectalivemarker){

        auto object = objectalivemarker.lock();

        return object.operator bool();
    }

    AliveMarkerT GetAliveMarker(){

        if(!IsAliveObjectOwner)
            IsAliveObjectOwner = std::make_shared<bool>(true);

        return IsAliveObjectOwner;
    }
    
private:

    std::shared_ptr<bool> IsAliveObjectOwner;
};

//! Makes checking for alive faster
#define INVOKE_CHECK_ALIVE_MARKER(x) { if(!IsAlive::IsStillAlive(x)){ \
    LOG_WARNING("Object no longer alive in Invoked function"); return; } }


//! Invokes a specific function really easily
#define INVOKE_FUNCTION_WITH_ALIVE_CHECK(funcname) { auto alive = this->GetAliveMarker();\
 DualView::Get().InvokeFunction([=](){\
 INVOKE_CHECK_ALIVE_MARKER(alive); \
 this->funcname();\
     });}

}
