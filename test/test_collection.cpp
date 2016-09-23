#include "catch.hpp"

#include "DummyLog.h"
#include "core/resources/Collection.h"

using namespace DV;

TEST_CASE("Collection name sanitization works", "[collection, file]") {

    Leviathan::TestLogger log("test_collection.txt");

    // Sanitization fatal asserts, or returns an empty string if it fails //

    SECTION("Simple cases"){

        SECTION("My backgrounds"){
            Collection collection("My backgrounds");

            CHECK(!collection.GetNameForFolder().empty());
        }

        SECTION("My cool stuff / funny things"){
            Collection collection("My cool stuff / funny things");

            CHECK(!collection.GetNameForFolder().empty());
        }

        SECTION("Just this | actually more stuff: here"){
            Collection collection("Just this | actually more stuff: here");

            CHECK(!collection.GetNameForFolder().empty());
        }
    }

    // These are some really trick things to check
    SECTION("Corner cases"){

        SECTION("Dots"){

            {
                Collection collection(".");
                CHECK(collection.GetNameForFolder() != ".");
            }

            {
                Collection collection("..");
                CHECK(collection.GetNameForFolder() != "..");
            }
        }
    }
}
