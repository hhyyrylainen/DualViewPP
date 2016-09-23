#include "catch.hpp"

#include "DummyLog.h"
#include "core/resources/Image.h"

using namespace DV;

TEST_CASE("File hash generation is correct", "[image, hash]") {

    Leviathan::TestLogger log("test_image.txt");

    Image img("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    CHECK(img.CalculateFileHash() == "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
    
}
