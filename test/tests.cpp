#include "catch.hpp"

#include "CacheManager.h"
#include "DownloadManager.h"
#include "DummyLog.h"
#include "Settings.h"
#include "TestDualView.h"
#include "VirtualPath.h"

#include "TimeHelpers.h"

#include "Common/StringOperations.h"

#include "json/json.h"

#include <memory>
#include <thread>

using namespace DV;

TEST_CASE("Cache Manager loads images", "[image][.expensive]")
{
    DV::TestDualView test;

    SECTION("Normal test image")
    {
        auto img =
            test.GetCacheManager().LoadFullImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

        REQUIRE(img);

        // Loop while loading //
        while(!img->IsLoaded()) {

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Check that it succeeded //
        CHECK(img->IsValid());

        CHECK(img->GetWidth() == 914);
        CHECK(img->GetHeight() == 1280);

        // Same object with the same path //
        auto img2 =
            test.GetCacheManager().LoadFullImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

        CHECK(img2.get() == img.get());
    }


    SECTION("Gif image")
    {
        auto img = test.GetCacheManager().LoadFullImage("data/bird bathing.gif");

        REQUIRE(img);

        // Loop while loading //
        while(!img->IsLoaded()) {

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

TEST_CASE("Settings right default stuff", "[random][settings]")
{
    DV::DummyDualView dv;
    DV::Settings settings("settings_test_settingsfile");

    CHECK(settings.GetDatabaseFile() == "./dualview.sqlite");
}

TEST_CASE("VirtualPath operations", "[path]")
{
    SECTION("Path combine works")
    {
        CHECK(VirtualPath() / VirtualPath("my folder") == VirtualPath("Root/my folder"));

        CHECK(VirtualPath() / VirtualPath("/my folder") == VirtualPath("Root/my folder"));

        CHECK(VirtualPath() / VirtualPath() == VirtualPath());

        CHECK(VirtualPath() / VirtualPath("a") == VirtualPath("Root/a"));
        CHECK(VirtualPath() / VirtualPath("/a") == VirtualPath("Root/a"));

        CHECK(VirtualPath() / VirtualPath("my folder/") == VirtualPath("Root/my folder/"));

        CHECK(
            VirtualPath() / VirtualPath("Root/my folder/") == VirtualPath("Root/my folder/"));

        CHECK(VirtualPath("Root/first - folder") / VirtualPath("/second") ==
              VirtualPath("Root/first - folder/second"));
    }

    SECTION("Up one folder works")
    {
        VirtualPath path1("Root/folder");

        path1.MoveUpOneFolder();

        CHECK(path1 == VirtualPath());

        CHECK(--VirtualPath("Root/first - folder/second") ==
              VirtualPath("Root/first - folder/"));

        CHECK(--VirtualPath("Root/first/second/") == VirtualPath("Root/first/"));
    }

    SECTION("Up multiple times works")
    {
        CHECK(--(--VirtualPath("Root/first/second/")) == VirtualPath("Root/"));
    }

    SECTION("Up and then combine")
    {
        CHECK(--VirtualPath("Root/first/second/") / VirtualPath("other") ==
              VirtualPath("Root/first/other"));
    }

    SECTION("Iterating")
    {
        SECTION("premade path")
        {
            VirtualPath path;

            SECTION("With trailing '/'")
            {
                path = VirtualPath("Root/my folder/other folder/last/");
            }

            SECTION("No trailing '/'")
            {
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

        SECTION("Going backwards from begin is end")
        {
            VirtualPath path("Root/folder");

            CHECK(--path.begin() == path.end());
        }
    }

    SECTION("Folder path resolve type prepending")
    {
        CHECK(static_cast<std::string>((VirtualPath() / VirtualPath(""))) == "Root/");

        CHECK(static_cast<std::string>((VirtualPath("") / VirtualPath())) == "Root/");

        CHECK(static_cast<std::string>((VirtualPath() / VirtualPath())) == "Root/");

        CHECK(static_cast<std::string>((VirtualPath() / VirtualPath())) == "Root/");
    }
}

TEST_CASE("Datetime parsing", "[db][time][.expensive]")
{
    DummyDualView dv;
    // const std::string original = "2016-09-18T20:07:49.7532581+03:00";
    const std::string original = "2016-09-18T20:07:49.753+03:00";

    REQUIRE_NOTHROW(TimeHelpers::parse8601(original));

    SECTION("Formatting yields original string")
    {
        auto time = TimeHelpers::parse8601(original);

        CHECK(TimeHelpers::format8601(time) == original);
    }
}

TEST_CASE("Filename from URL", "[url][download]")
{
    SECTION("Normal names")
    {
        CHECK(DownloadManager::ExtractFileName(
                  "http://w.com//images/eb/3f/eb3f8e3a01665cc99794bb7017dd5b92.jpg?3427768") ==
              "eb3f8e3a01665cc99794bb7017dd5b92.jpg");

        CHECK(DownloadManager::ExtractFileName("http://i.imgur.com/AF7pCun.jpg") ==
              "AF7pCun.jpg");

        CHECK(DownloadManager::ExtractFileName(
                  "http://x.abs.com/u/ufo/6495436/263030533/82.jpg") == "82.jpg");
    }

    SECTION("Unescaping stuff")
    {
        CHECK(DownloadManager::ExtractFileName(
                  "http://normalsite.com/contents/My%20cool%20image%20%3Ahere.jpg") ==
              "My cool image :here.jpg");
    }

    SECTION("Sneaky slashes")
    {
        CHECK(DownloadManager::ExtractFileName(
                  "http://normalsite.com/contents/My%20cool%20image%20%3%2Ahere.jpg")
                  .find_first_of('/') == std::string::npos);
    }

    SECTION("Real world examples")
    {
        CHECK(DownloadManager::ExtractFileName(
                  "http://x.site.com/u/usrname/6525068/348430179/04_thief.jpg") ==
              "04_thief.jpg");
    }
}

TEST_CASE("CacheManager database path translations", "[path][db]")
{
    DV::MemorySettingsDualView dv;
    REQUIRE(dv.GetSettings().GetPrivateCollection() == "./private_collection/");

    REQUIRE(Leviathan::StringOperations::StringStartsWith<std::string>(
        "./private_collection/collections/users data/image1.jpg", "./private_collection/"));

    SECTION("Basic valid things")
    {
        CHECK(CacheManager::GetDatabaseImagePath(
                  "./private_collection/collections/users data/image1.jpg") ==
              ":?scl/collections/users data/image1.jpg");

        CHECK(CacheManager::GetDatabaseImagePath(
                  "./public_collection/collections/users data/image1.jpg") ==
              ":?ocl/collections/users data/image1.jpg");

        CHECK(CacheManager::GetFinalImagePath(":?ocl/collections/users data/image1.jpg") ==
              "./public_collection/collections/users data/image1.jpg");

        CHECK(CacheManager::GetFinalImagePath(":?scl/collections/users data/image1.jpg") ==
              "./private_collection/collections/users data/image1.jpg");
    }

    SECTION("legacy paths")
    {
        CHECK(CacheManager::GetFinalImagePath(
                  "./public_collection/collections/users data/image1.jpg") ==
              "./public_collection/collections/users data/image1.jpg");

        CHECK(CacheManager::GetFinalImagePath(
                  "./private_collection/collections/users data/image1.jpg") ==
              "./private_collection/collections/users data/image1.jpg");
    }
}

TEST_CASE("JSON serialization works like it should")
{
    std::stringstream sstream;
    Json::Value value;

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    auto writer = builder.newStreamWriter();

    Json::Value images;
    images.resize(2);
    for(size_t i = 0; i < images.size(); ++i) {
        images[static_cast<unsigned>(i)] = i + 2;
    }

    Json::Value tags;
    tags.resize(3);
    for(size_t i = 0; i < tags.size(); ++i) {
        tags[static_cast<unsigned>(i)] = i * 2;
    }

    Json::Value collections;
    collections.resize(4);
    for(size_t i = 0; i < collections.size(); ++i) {
        collections[static_cast<unsigned>(i)]["collection"] = i * 20;
        collections[static_cast<unsigned>(i)]["order"] = i;
    }

    value["images"] = images;
    value["target"] = 1;
    value["tags"] = tags;
    value["collections"] = collections;

    writer->write(value, &sstream);

    CHECK(R"({"collections":[{"collection":0,"order":0},{"collection":20,"order":1},)"
          R"({"collection":40,"order":2},{"collection":60,"order":3}],"images":[2,3],)"
          R"("tags":[0,2,4],"target":1})" == sstream.str());
}
