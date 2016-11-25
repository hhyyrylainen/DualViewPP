#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"

#include "core/TimeHelpers.h"
#include "core/UtilityHelpers.h"

#include "leviathan/Common/StringOperations.h"

#include <thread>
#include <memory>

using namespace DV;

void TestCompRequirements(const std::string &a, const std::string &b, const std::string &c,
    const std::string &str)
{
    REQUIRE(a != b);
    REQUIRE(a != c);
    REQUIRE(b != c);

    //comp(a, b)
    CHECK(DV::CompareSuggestionStrings(str, a, a) == false);

    if(DV::CompareSuggestionStrings(str, a, b) == true){

        CHECK(DV::CompareSuggestionStrings(str, b, a) == false);
    }

    if(DV::CompareSuggestionStrings(str, a, b) == true &&
        DV::CompareSuggestionStrings(str, b, c) == true)
    {

        CHECK(DV::CompareSuggestionStrings(str, a, c) == true);
    }

    //equiv(a, b)
    // equiv = !comp(a, b) && !comp(b, a)

    CHECK((!DV::CompareSuggestionStrings(str, a, a) &&
            !DV::CompareSuggestionStrings(str, a, a)) == true);

    if((!DV::CompareSuggestionStrings(str, a, b) &&
            !DV::CompareSuggestionStrings(str, b, a)))
    {
        CHECK((!DV::CompareSuggestionStrings(str, b, a) &&
                !DV::CompareSuggestionStrings(str, a, b)));
    }

    if((!DV::CompareSuggestionStrings(str, a, b) &&
            !DV::CompareSuggestionStrings(str, b, a)) &&
        (!DV::CompareSuggestionStrings(str, b, c) &&
            !DV::CompareSuggestionStrings(str, c, b)))
    {
        CHECK((!DV::CompareSuggestionStrings(str, a, c) &&
                !DV::CompareSuggestionStrings(str, c, a)));
    }
}

TEST_CASE("Suggestions sort works correctly", "[sort][suggestion][db][helper]"){

    SECTION("Compare predicate requirements"){

        TestCompRequirements("my str", "mthought", "jun", "my");

        TestCompRequirements("asgfasg", "asikfg", "469807djl", "fa");

        TestCompRequirements("random string 1", "random string 2", "random string 3",
            "random");
    }
}
