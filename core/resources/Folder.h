#pragma once

#include "DatabaseResource.h"

#include <string>

namespace DV{

class PreparedStatement;

class Folder : public DatabaseResource{
public:
    
    //! \brief Database load function
    Folder(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);

protected:

    // DatabaseResource implementation
    void _DoSave(Database &db) override;

protected:

    std::string Name;

    bool IsPrivate = false;
};

}
