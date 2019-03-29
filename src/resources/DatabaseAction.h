#pragma once

#include "DatabaseResource.h"
#include "ReversibleAction.h"

namespace DV {

//! \brief All reversible database operations result in this that can then be used to reverse
//! it
//!
//! The action data is stored as JSON in the database and this class doesn't really understand
//! what this contains. The Database is given the JSON data for all processing
class DatabaseAction : public ReversibleAction, public DatabaseResource {
public:
};

} // namespace DV
