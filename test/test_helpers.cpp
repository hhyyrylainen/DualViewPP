#include "catch.hpp"

#include "TestDualView.h"

#include "TimeHelpers.h"
#include "UtilityHelpers.h"

#include "Common/StringOperations.h"

#include <memory>

using namespace DV;

void TestCompRequirements(
    const std::string &a, const std::string &b, const std::string &c, const std::string &str) {
    REQUIRE(a != b);
    REQUIRE(a != c);
    REQUIRE(b != c);

    // comp(a, b)
    CHECK(CompareSuggestionStrings(str, a, a) == false);

    if (CompareSuggestionStrings(str, a, b) == true) {
        CHECK(CompareSuggestionStrings(str, b, a) == false);
    }

    if (CompareSuggestionStrings(str, a, b) == true &&
        CompareSuggestionStrings(str, b, c) == true) {
        CHECK(CompareSuggestionStrings(str, a, c) == true);
    }

    // equiv(a, b)
    // equiv = !comp(a, b) && !comp(b, a)

    CHECK((!CompareSuggestionStrings(str, a, a) &&
        !CompareSuggestionStrings(str, a, a)) == true);

    if ((!CompareSuggestionStrings(str, a, b) &&
         !CompareSuggestionStrings(str, b, a))) {
        CHECK((!CompareSuggestionStrings(str, b, a) &&
            !CompareSuggestionStrings(str, a, b)));
    }

    if ((!CompareSuggestionStrings(str, a, b) &&
         !CompareSuggestionStrings(str, b, a)) &&
        (!CompareSuggestionStrings(str, b, c) &&
         !CompareSuggestionStrings(str, c, b))) {
        CHECK((!CompareSuggestionStrings(str, a, c) &&
            !CompareSuggestionStrings(str, c, a)));
    }
}

TEST_CASE("Suggestions sort works correctly", "[sort][suggestion][db][helper]") {
    SECTION("Compare predicate requirements") {
        TestCompRequirements("my str", "mthought", "jun", "my");

        TestCompRequirements("asgfasg", "asikfg", "469807djl", "fa");

        TestCompRequirements(
            "random string 1", "random string 2", "random string 3", "random");
    }

    SECTION("Name prefix stuff") {
        const auto matchStr = "rebel";

        CHECK(CompareSuggestionStrings(matchStr, "short rebel", "a really long rebel"));
    }
}

TEST_CASE("File path comparison works", "[sort][helper]") {
    SECTION("Basic items") {
        const auto first = "item.jpg";
        const auto second = "item2.jpg";

        CHECK(CompareFilePaths(first, second));
        CHECK(!CompareFilePaths(second, first));
    }

    SECTION("Number comparison") {
        SECTION("Plain") {
            const auto first = "3.jpg";
            const auto second = "10.jpg";

            CHECK(CompareFilePaths(first, second));
            CHECK(!CompareFilePaths(second, first));
        }

        SECTION("as a suffix") {
            const auto first = "img_3.jpg";
            const auto second = "img_10.jpg";

            CHECK(CompareFilePaths(first, second));
            CHECK(!CompareFilePaths(second, first));
        }

        SECTION("suffix without separator") {
            const auto first = "prefix4008.jpg";
            const auto second = "prefix04013.jpg";

            CHECK(CompareFilePaths(first, second));
            CHECK(!CompareFilePaths(second, first));
        }

        SECTION("different length prefix") {
            const auto first = "prefix4008.jpg";
            const auto second = "prefix2_05.jpg";

            CHECK(CompareFilePaths(first, second));
            CHECK(!CompareFilePaths(second, first));
        }

        SECTION("Extra number in parentheses") {
            // Optimally these would go the other way but that would require extra detection
            // logic for " (number)"
            const auto first = "020 (2).jpg";
            const auto second = "020.jpg";

            CHECK(CompareFilePaths(first, second));
            CHECK(!CompareFilePaths(second, first));
        }
    }

    SECTION("Inside a folder") {
        const auto first = "folder/item.jpg";
        const auto second = "folder/item2.jpg";

        CHECK(CompareFilePaths(first, second));
        CHECK(!CompareFilePaths(second, first));
    }

    SECTION("Different nesting levels") {
        const auto first = "b.jpg";
        const auto second = "folder/a.jpg";

        CHECK(CompareFilePaths(first, second));
        CHECK(!CompareFilePaths(second, first));
    }

    SECTION("Blank filename") {
        const auto first = ".ajpg";
        const auto second = ".jpg";

        CHECK(CompareFilePaths(first, second));
        CHECK(!CompareFilePaths(second, first));
    }
}

TEST_CASE("File path list sorting works", "[sort][helper]") {
    SECTION("Basic single folder contents") {
        std::vector<std::string> input{
            "/some/folder/3.jpg", "/some/folder/1.jpg",
            "/some/folder/10.jpg", "/some/folder/2.jpg"
        };

        REQUIRE_NOTHROW(SortFilePaths(input.begin(), input.end()));
        CHECK(input == std::vector<std::string>{"/some/folder/1.jpg", "/some/folder/2.jpg",
              "/some/folder/3.jpg", "/some/folder/10.jpg"});
    }

    SECTION("Real folder test") {
        std::vector<std::string> input{
            "Folder/28.jpg", "Folder/1.jpg", "Folder/10.jpg",
            "Folder/11.jpg", "Folder/12.jpg", "Folder/13.jpg", "Folder/14.jpg",
            "Folder/15.jpg", "Folder/16.jpg", "Folder/17.jpg", "Folder/18.jpg",
            "Folder/19.jpg", "Folder/2.jpg", "Folder/20.jpg", "Folder/21.jpg", "Folder/22.jpg",
            "Folder/23.jpg", "Folder/24.jpg", "Folder/25.jpg", "Folder/26.jpg",
            "Folder/27.jpg", "Folder/29.jpg", "Folder/3.jpg"
        };

        std::vector<std::string> result{
            "Folder/1.jpg", "Folder/2.jpg", "Folder/3.jpg",
            "Folder/10.jpg", "Folder/11.jpg", "Folder/12.jpg", "Folder/13.jpg",
            "Folder/14.jpg", "Folder/15.jpg", "Folder/16.jpg", "Folder/17.jpg",
            "Folder/18.jpg", "Folder/19.jpg",

            "Folder/20.jpg", "Folder/21.jpg", "Folder/22.jpg", "Folder/23.jpg",
            "Folder/24.jpg", "Folder/25.jpg", "Folder/26.jpg", "Folder/27.jpg",
            "Folder/28.jpg", "Folder/29.jpg"
        };

        REQUIRE_NOTHROW(SortFilePaths(input.begin(), input.end()));
        CHECK(input == result);
    }

    SECTION("Different length prefixes before numbers") {
        std::vector<std::string> input{
            "prefix4008.jpg", "prefix2_05.jpg", "prefix2_06.jpg",
            "prefix04013.jpg", "013.jpg", "010.jpg", "014.jpg"
        };

        std::vector<std::string> result{
            "010.jpg", "013.jpg", "014.jpg", "prefix4008.jpg",
            "prefix04013.jpg", "prefix2_05.jpg", "prefix2_06.jpg"
        };

        REQUIRE_NOTHROW(SortFilePaths(input.begin(), input.end()));
        CHECK(input == result);
    }

    SECTION("Number in parentheses") {
        std::vector<std::string> input{"020 (2).jpg", "019.jpg", "020.jpg"};

        std::vector<std::string> result{"019.jpg", "020 (2).jpg", "020.jpg"};

        REQUIRE_NOTHROW(SortFilePaths(input.begin(), input.end()));
        CHECK(input == result);
    }
}

TEST_CASE("URL combine edge cases work correctly", "[url][helper]") {
    SECTION("Second query strings tarts with 'q'") {
        SECTION("Combine second part that starts with '?'") {
            CHECK(Leviathan::StringOperations::CombineURL( "https://example.com/thing", "?page=2") ==
                "https://example.com/thing?page=2");

            CHECK(Leviathan::StringOperations::CombineURL( "https://example.com/thing?old_query", "?page=2") ==
                "https://example.com/thing?page=2");

            // Not sure if this should be like this or end with "/?page=2"
            CHECK(Leviathan::StringOperations::CombineURL( "https://example.com/thing/", "?page=2") ==
                "https://example.com/thing?page=2");
        }
    }
}
