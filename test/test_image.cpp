#include "catch.hpp"

#include "CacheManager.h"
#include "DummyLog.h"
#include "Settings.h"
#include "TestDualView.h"
#include "resources/Image.h"

#include <Magick++.h>
#include <boost/filesystem.hpp>

using namespace DV;

TEST_CASE("Image getptr works", "[image]")
{
    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    auto img2 = img->GetPtr();

    CHECK(img.get() == img2.get());
    CHECK(img.use_count() == img2.use_count());
}

TEST_CASE("File hash generation is correct", "[image][hash]")
{
    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    CHECK(img->CalculateFileHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
}

TEST_CASE("ImageMagick properly loads the test image", "[image]")
{
    SECTION("jpg")
    {
        std::shared_ptr<std::vector<Magick::Image>> imageobj;

        REQUIRE_NOTHROW(
            LoadedImage::LoadImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg", imageobj));

        REQUIRE(imageobj);

        std::vector<Magick::Image>& images = *imageobj;

        REQUIRE(images.size() > 0);
        CHECK(images.size() == 1);

        Magick::Image& firstImage = images.front();

        // Verify size //
        CHECK(firstImage.columns() == 914);
        CHECK(firstImage.rows() == 1280);
    }

    SECTION("gif")
    {
        std::shared_ptr<std::vector<Magick::Image>> imageobj;

        REQUIRE_NOTHROW(LoadedImage::LoadImage("data/bird bathing.gif", imageobj));

        REQUIRE(imageobj);

        std::vector<Magick::Image>& images = *imageobj;

        REQUIRE(images.size() > 0);
        CHECK(images.size() == 142);

        Magick::Image& firstImage = images.front();

        // Verify size //
        CHECK(firstImage.columns() == 250);
        CHECK(firstImage.rows() == 250);
    }
}


TEST_CASE("File hash calculation happens on a worker thread", "[image][hash]")
{
    TestDualView dualview("test_image.sqlite");

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    int failCount = 0;

    while(!img->IsReady()) {

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    CHECK(img->GetHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
}

TEST_CASE("Thumbnail generation does something", "[image]")
{
    TestDualView dualview("test_image.sqlite");

    // Recreate thumbnail folder //
    auto folder = boost::filesystem::path(dualview.GetThumbnailFolder());

    if(boost::filesystem::exists(folder)) {

        boost::filesystem::remove_all(folder);
    }

    boost::filesystem::create_directories(folder);

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    int failCount = 0;

    while(!img->IsReady()) {

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    // Get thumbnail //
    auto thumb = img->GetThumbnail();

    while(!thumb->IsLoaded()) {

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    CHECK(thumb->IsValid());
    CHECK(
        boost::filesystem::exists(folder / boost::filesystem::path(img->GetHash() + ".jpg")));
}

TEST_CASE("Thumbnail for gif has fewer frames", "[image][expensive]")
{
    TestDualView dualview("test_image.sqlite");

    // Recreate thumbnail folder //
    auto folder = boost::filesystem::path(dualview.GetThumbnailFolder());

    if(boost::filesystem::exists(folder)) {

        boost::filesystem::remove_all(folder);
    }

    boost::filesystem::create_directories(folder);

    auto img = Image::Create("data/bird bathing.gif");

    int failCount = 0;

    while(!img->IsReady()) {

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    // Get thumbnail //
    auto thumb = img->GetThumbnail();

    while(!thumb->IsLoaded()) {

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    CHECK(thumb->IsValid());
    CHECK(
        boost::filesystem::exists(folder / boost::filesystem::path(img->GetHash() + ".gif")));

    CHECK(thumb->GetFrameCount() <= 142 / 2);
}

TEST_CASE("Thumbnail is created in a different folder", "[image][expensive]")
{
    DV::TestDualView dualview("test_image.sqlite");

    dualview.GetSettings().SetPrivateCollection("new-folder-thumbnails");
    boost::filesystem::remove_all("new-folder-thumbnails");
    boost::filesystem::create_directories(dualview.GetThumbnailFolder());

    auto img = DV::Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    while(!img->IsReady()) {

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(img->IsReady());

    auto thumb = img->GetThumbnail();

    while(!thumb->IsLoaded()) {

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE(thumb->IsValid());

    auto path = boost::filesystem::path(dualview.GetThumbnailFolder()) /
                boost::filesystem::path(img->GetHash() + ".jpg");
    INFO("Path is: " << path);
    INFO("NOTE: there seems to be a file creation race condition in this test. ");
    REQUIRE(boost::filesystem::exists(path));
}
