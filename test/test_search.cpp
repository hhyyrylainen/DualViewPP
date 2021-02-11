#include "catch.hpp"

#include "TestDualView.h"
#include "TestDatabase.h"

#include "resources/Collection.h"

using namespace DV;

TEST_CASE("Collection name auto complete works", "[collection][search]") {
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    const auto collection1 = db.InsertCollectionAG("Collection 1", false);
    const auto collection2 =db.InsertCollectionAG("Collection 2", false);
    const auto collection3 =db.InsertCollectionAG("Another collection", false);
    const auto collection4 =db.InsertCollectionAG("Something else", false);

    SECTION("Basic")
    {
        const auto results = db.SelectCollectionNamesByWildcard("Colle");

        CHECK(std::find(results.begin(), results.end(), collection1->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection2->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection3->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection4->GetName()) == results.end());
    }

    SECTION("Non-case sensitive"){
        const auto results = db.SelectCollectionNamesByWildcard("col");

        CHECK(std::find(results.begin(), results.end(), collection1->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection2->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection3->GetName()) != results.end());
        CHECK(std::find(results.begin(), results.end(), collection4->GetName()) == results.end());
    }

    SECTION("Ordered by match goodness"){
        const auto results = db.SelectCollectionNamesByWildcard("col", 2);

        CHECK(results == std::vector<std::string>{"Collection 1", "Collection 2"});
    }
}

