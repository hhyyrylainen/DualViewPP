// ------------------------------------ //
#include "DatabaseResource.h"

#include "core/Database.h"

#include "core/DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
DatabaseResource::DatabaseResource(bool notloadedfromdb){

}

//! \brief Database calls this constructor (indirectly) when loading
DatabaseResource::DatabaseResource(int64_t id, Database &from) : ID(id), InDatabase(true)
{
    LEVIATHAN_ASSERT(id != -1, "Loaded resource has ID of -1");
}
    
DatabaseResource::~DatabaseResource(){

    // Stop listeners from receiving updates
    ReleaseChildHooks();
    
    Save();
}
// ------------------------------------ //
void DatabaseResource::Save(){

    if(!IsDirty)
        return;

    try{
        
        _DoSave(DualView::Get().GetDatabase());
    
    } catch(const InvalidSQL &e){

        LOG_ERROR("DatabaseResource: failed to save, exception: ");
        e.PrintToLog();
        return;
    }
    
    IsDirty = false;
}
