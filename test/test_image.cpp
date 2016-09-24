#include "catch.hpp"

#include "DummyLog.h"
#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include <Magick++.h>

using namespace DV;

TEST_CASE("File hash generation is correct", "[image, hash]") {

    Leviathan::TestLogger log("test_image.txt");

    Image img("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    CHECK(img.CalculateFileHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
    
}

TEST_CASE("ImageMagick properly loads the test image", "[image]"){


    std::shared_ptr<std::list<Magick::Image>> imageobj;
    
    REQUIRE_NOTHROW(LoadedImage::LoadImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            imageobj));

    REQUIRE(imageobj);

    std::list<Magick::Image>& images = *imageobj;

    REQUIRE(images.size() > 0);
    CHECK(images.size() == 1);

    Magick::Image& firstImage = images.front();

    // Verify size //
    CHECK(firstImage.columns() == 914);
    CHECK(firstImage.rows() == 1280);
    

    
}
