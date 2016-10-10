#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "core/CacheManager.h"
#include "core/Settings.h"
#include "core/VirtualPath.h"

#include <thread>
#include <memory>

using namespace DV;

TEST_CASE("Cache Manager loads images", "[image]"){

    DV::TestDualView test;

    SECTION("Normal test image"){
        auto img = test.GetCacheManager().LoadFullImage(
            "data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

        REQUIRE(img);

        // Loop while loading //
        while(!img->IsLoaded()){

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Check that it succeeded //
        CHECK(img->IsValid());

        CHECK(img->GetWidth() == 914);
        CHECK(img->GetHeight() == 1280);

        // Same object with the same path //
        auto img2 = test.GetCacheManager().LoadFullImage(
            "data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

        CHECK(img2.get() == img.get());
    }


    SECTION("Gif image"){
        
        auto img = test.GetCacheManager().LoadFullImage(
            "data/bird bathing.gif");

        REQUIRE(img);

        // Loop while loading //
        while(!img->IsLoaded()){

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Check that it succeeded //
        CHECK(img->IsValid());

        CHECK(img->GetWidth() == 250);
        CHECK(img->GetHeight() == 250);

        // Page count //
        CHECK(img->GetFrameCount() == 142);
    }
}

TEST_CASE("Settings right default stuff", "[random][settings]"){

    DV::DummyDualView dv;
    DV::Settings settings("settings_test_settingsfile");

    CHECK(settings.GetDatabaseFile() == "./dualview.sqlite");
}

TEST_CASE("VirtualPath operations", "[path]"){

    SECTION("Path combine works"){

        CHECK(VirtualPath() / VirtualPath("my folder") == VirtualPath("Root/my folder"));

        CHECK(VirtualPath() / VirtualPath("/my folder") == VirtualPath("Root/my folder"));

        CHECK(VirtualPath() / VirtualPath() == VirtualPath());

        CHECK(VirtualPath() / VirtualPath("a") == VirtualPath("Root/a"));
        CHECK(VirtualPath() / VirtualPath("/a") == VirtualPath("Root/a"));

        CHECK(VirtualPath() / VirtualPath("my folder/") == VirtualPath("Root/my folder/"));

        CHECK(VirtualPath() / VirtualPath("Root/my folder/") ==
            VirtualPath("Root/my folder/"));

        CHECK(VirtualPath("Root/first - folder") / VirtualPath("/second") ==
            VirtualPath("Root/first - folder/second"));
    }

    SECTION("Up one folder works"){

        VirtualPath path1("Root/folder");

        path1.MoveUpOneFolder();

        CHECK(path1 == VirtualPath());

        CHECK(--VirtualPath("Root/first - folder/second") ==
            VirtualPath("Root/first - folder/"));

        CHECK(--VirtualPath("Root/first/second/") == VirtualPath("Root/first/"));
    }

    SECTION("Up multiple times works"){

        CHECK(--(--VirtualPath("Root/first/second/")) == VirtualPath("Root/"));
    }

    SECTION("Up and then combine"){

        CHECK(--VirtualPath("Root/first/second/") / VirtualPath("other") ==
            VirtualPath("Root/first/other"));
    }

    SECTION("Iterating"){

        SECTION("premade path"){

            VirtualPath path;
            
            SECTION("With trailing '/'"){

                path = VirtualPath("Root/my folder/other folder/last/");
            }

            SECTION("No trailing '/'"){

                path = VirtualPath("Root/my folder/other folder/last");
            }

            auto iter = path.begin();

            CHECK(iter != path.end());
            CHECK(path.end() == path.end());

            CHECK(*iter == "Root");
            
            CHECK(++iter != path.end());
            CHECK(*iter == "my folder");

            CHECK(++iter != path.end());
            CHECK(*iter == "other folder");

            CHECK(++iter != path.end());
            CHECK(*iter == "last");

            CHECK(++iter == path.end());
            CHECK(*iter == "");
            
        }

        SECTION("Going backwards from begin is end"){

            VirtualPath path("Root/folder");

            CHECK(--path.begin() == path.end());
        }
    }
}
