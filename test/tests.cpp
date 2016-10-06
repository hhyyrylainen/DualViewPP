#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "core/CacheManager.h"
#include "core/Settings.h"

#include <thread>
#include <memory>


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
