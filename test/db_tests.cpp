#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "Common.h"

#include "core/Database.h"

#include <thread>
#include <memory>

#include <iostream>

using namespace DV;

TEST_CASE("Sqlite basic thing works", "[db]"){

    
}

TEST_CASE("Database in memory creation", "[db]"){

    Database db(true);
    
}

