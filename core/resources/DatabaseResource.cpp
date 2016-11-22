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
DatabaseResource::DatabaseResource(int64_t id, Database &from) : ID(id), InDatabase(&from)
{
    LEVIATHAN_ASSERT(id != -1, "Loaded resource has ID of -1");
}

DatabaseResource::~DatabaseResource(){

    LEVIATHAN_ASSERT(_DestructCalled, "DatabaseResource child class has "
        "not called DBResourceDestruct");
}

void DatabaseResource::DBResourceDestruct(){

    // Stop listeners from receiving updates
    ReleaseChildHooks();
    
    Save();

    _DestructCalled = true;
}
// ------------------------------------ //
void DatabaseResource::OnAdopted(int64_t id, Database &from){

    LEVIATHAN_ASSERT(id != -1, "Adopted resource has ID of -1");

    ID = id;
    InDatabase = &from;
    IsDirty = false;

    _OnAdopted();
}

void DatabaseResource::_OnAdopted(){
}

void DatabaseResource::_BecomeDuplicateOf(const DatabaseResource &other){

    LEVIATHAN_ASSERT(other.ID != -1, "To be duplicated resource has ID of -1");

    ID = other.ID;
    InDatabase = other.InDatabase;
    
    IsDirty = false;
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
