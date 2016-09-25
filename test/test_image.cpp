#include "catch.hpp"

#include "DummyLog.h"
#include "TestDualView.h"
#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include <Magick++.h>

using namespace DV;

TEST_CASE("Image getptr works", "[image]"){

    DummyDualView dummy;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    auto img2 = img->GetPtr();

    CHECK(img.get() == img2.get());
    CHECK(img.use_count() == img2.use_count());
}

TEST_CASE("File hash generation is correct", "[image, hash]") {

    DummyDualView dummy;

    auto  img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    CHECK(img->CalculateFileHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
}

TEST_CASE("ImageMagick properly loads the test image", "[image]"){

    std::shared_ptr<std::vector<Magick::Image>> imageobj;
    
    REQUIRE_NOTHROW(LoadedImage::LoadImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            imageobj));

    REQUIRE(imageobj);

    std::vector<Magick::Image>& images = *imageobj;

    REQUIRE(images.size() > 0);
    CHECK(images.size() == 1);

    Magick::Image& firstImage = images.front();

    // Verify size //
    CHECK(firstImage.columns() == 914);
    CHECK(firstImage.rows() == 1280);
    

    
}


TEST_CASE("File hash calculation happens on a worker thread", "[image, hash]"){
    
    TestDualView dualview;

    auto img = Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    int failCount = 0;

    while(!img->IsReady()){

        ++failCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Fail after 30 seconds //
        REQUIRE(failCount < 6000);
    }

    CHECK(img->GetHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
}
