#include "catch.hpp"

#include "CacheManager.h"
#include "DummyLog.h"
#include "Settings.h"
#include "SignatureCalculator.h"
#include "TestDualView.h"
#include "resources/Image.h"

#include <Magick++.h>
#include <boost/filesystem.hpp>

using namespace DV;

TEST_CASE("Image getptr works", "[image][.expensive]")
{
    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    auto img2 = img->GetPtr();

    CHECK(img.get() == img2.get());
    CHECK(img.use_count() == img2.use_count());
}

TEST_CASE("File hash generation is correct", "[image][hash][.expensive]")
{
    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    CHECK(img->CalculateFileHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
}

TEST_CASE("ImageMagick properly loads the test image", "[image][.expensive]")
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


TEST_CASE("File hash calculation happens on a worker thread", "[image][hash][.expensive]")
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

TEST_CASE("Thumbnail generation does something", "[image][.expensive]")
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Fail after 30 seconds //
        REQUIRE(failCount < 3000);
    }

    CHECK(thumb->IsValid());
    CHECK(
        boost::filesystem::exists(folder / boost::filesystem::path(img->GetHash() + ".jpg")));
}

TEST_CASE("Thumbnail for gif has fewer frames", "[image][.expensive]")
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

TEST_CASE("Thumbnail is created in a different folder", "[image][.expensive]")
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

TEST_CASE("Image signature calculation on non-db image works", "[image][hash][.expensive]")
{
    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    SignatureCalculator calculator;
    CHECK(calculator.CalculateImageSignature(*img));

    CHECK(
        img->GetSignatureBase64() ==
        "Af/////+/v8B//8B/wECAQEC/v7+/wEC/gEC/gICAgEB/v7+//8CAQEBAgECAgECAf8BAQL/Af////8B/v4B/"
        "wIBAv//Av4B//4B/v////7+//7//v4C/wICAQIBAv8B//8B/wL/Af//Af/+/wH+/v7+///+/v7/AQIBAv//"
        "AQICAQIBAgICAv4BAf7//gEAAQL+Af4CAf4C/wECAgL/Af7+//7+/v7+AQECAQEBAv8CAgICAQECAgL/Af/+/"
        "gEB/v7+/gL/Av7+Af7//v7//v//AP/+Af7/Af4CAQICAgICAv8C//4AAQEBAv8AAQH+/v/+/v7+/wH+/v//"
        "AQICAgACAgICAgIAAgICAv8BAf4B/wIAAf/+//4B//4C/v8CAQH+AP7+/////v/+//8BAf8CAgECAgIB/"
        "v8AAgH/Af7+AAEB/v7+/gH/Av7+Af7+//7+/v4BAAH/Av7/Av8CAgICAgICAgAB//7/AQEBAgEBAQH+/v/+//"
        "7//wH+/wH/AQICAv8CAgICAgIBAgICAv8CAf4C/wL///7+/v4A/v4B/v8CAQH+Af7//////gD///"
        "8BAf8CAQIBAQH//v7/////Af7+AQEA//7+/gH/Av/+Af7+AP7+/v4BAQL/Av4BAv8CAgICAgICAv8A//7////"
        "/AQEBAQH/AQH/AQD//wEBAQH+AgICAgL/Av/+Av/+/v7+Af4BAv/+Af8BAAH/AA==");
}

TEST_CASE("Image resize for thumbnail size works", "[image][thumbnail]")
{
    SECTION("Specified width")
    {
        CHECK(CacheManager::CreateResizeSizeForImage(1920, 1080, 128, 0) == "128x72");
        CHECK(CacheManager::CreateResizeSizeForImage(512, 512, 128, 0) == "128x128");
        CHECK(CacheManager::CreateResizeSizeForImage(1632, 1900, 128, 0) == "128x109");
    }

    SECTION("Specified height")
    {
        CHECK(CacheManager::CreateResizeSizeForImage(1920, 1080, 0, 192) == "108x192");
        CHECK(CacheManager::CreateResizeSizeForImage(512, 512, 0, 128) == "128x128");
        CHECK(CacheManager::CreateResizeSizeForImage(1632, 1900, 0, 128) == "109x128");
    }

    SECTION("Really big difference")
    {
        CHECK(CacheManager::CreateResizeSizeForImage(480, 19080, 128, 0) == "128x3");
        CHECK(CacheManager::CreateResizeSizeForImage(480, 190800, 128, 0) == "128x1");
    }
}

TEST_CASE("Non-animated extension detection works", "[image][thumbnail]")
{
    SECTION("Image.png")
    {
        const std::string extension{".png"};

        CHECK(std::find(ANIMATED_IMAGE_EXTENSIONS.begin(), ANIMATED_IMAGE_EXTENSIONS.end(),
                  extension) == ANIMATED_IMAGE_EXTENSIONS.end());
    }

    SECTION("gif is found")
    {
        const std::string extension{".gif"};

        CHECK(std::find(ANIMATED_IMAGE_EXTENSIONS.begin(), ANIMATED_IMAGE_EXTENSIONS.end(),
                  extension) != ANIMATED_IMAGE_EXTENSIONS.end());
    }
}
