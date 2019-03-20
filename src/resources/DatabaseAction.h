#pragma once

#include "DatabaseResource.h"
#include "ReversibleAction.h"

namespace DV {

//! \brief Base class for all reversible database operations
class BaseDatabaseAction : public ReversibleAction, public DatabaseResource {
public:
};

} // namespace DV
