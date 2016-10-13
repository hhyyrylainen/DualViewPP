// ------------------------------------ //
#include "Folder.h"

#include "core/Database.h"
#include "core/PreparedStatement.h"

using namespace DV;
// ------------------------------------ //

Folder::Folder(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id) :

    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "is_private");

    Name = statement.GetColumnAsString(1);
    IsPrivate = statement.GetColumnAsBool(2);
}
// ------------------------------------ //
void Folder::_DoSave(Database &db){

    db.UpdateFolder(*this);
}
