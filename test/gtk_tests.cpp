#include "catch.hpp"

#include "CacheManager.h"
#include "Database.h"
#include "DummyLog.h"
#include "Settings.h"
#include "TestDualView.h"
#include "components/SuperContainer.h"
#include "resources/Collection.h"
#include "resources/Image.h"
#include "resources/Tags.h"

#include "Common.h"

#include <Magick++.h>

#include <memory>
#include <thread>

#include <boost/filesystem.hpp>

#include <iostream>
#include <random>

using namespace DV;

//! \file All tests that require gtk to be initialized

//! \brief Singleton keeping a gtk connection open, as it seems you can no longer init this
//! multiple times
class GtkTestsFixture {
public:
    GtkTestsFixture()
    {
        app = Gtk::Application::create("com.boostslair.dualview.tests.test");

        REQUIRE(app->register_application());
    }

protected:
    Glib::RefPtr<Gtk::Application> app;
};

GtkTestsFixture& GetGtkFixture()
{
    static GtkTestsFixture fixture;
    return fixture;
}


//#define PRINT_PIXEL_VALUES

void CheckPixel(Glib::RefPtr<Gdk::Pixbuf>& pixbuf, Magick::Image& image, size_t x, size_t y)
{
    const auto& magickColour = image.pixelColor(x, y);
    auto* pixels = pixbuf->get_pixels();
    const auto gtkRed = pixels[(x * 3) + (y * pixbuf->get_rowstride())];
    const auto gtkGreen = pixels[(x * 3) + (y * pixbuf->get_rowstride()) + 1];
    const auto gtkBlue = pixels[(x * 3) + (y * pixbuf->get_rowstride()) + 2];

    const auto magickRed = MagickCore::ScaleQuantumToChar(magickColour.quantumRed());
    const auto magickGreen = MagickCore::ScaleQuantumToChar(magickColour.quantumGreen());
    const auto magickBlue = MagickCore::ScaleQuantumToChar(magickColour.quantumBlue());

#ifdef PRINT_PIXEL_VALUES
    std::cout << "Comparing: " << (int)magickRed << ", " << (int)magickGreen << ", "
              << (int)magickBlue << std::endl;

    std::cout << "With     : " << (int)gtkRed << ", " << (int)gtkGreen << ", " << (int)gtkBlue
              << std::endl;
#endif

    if(gtkRed != magickRed) {

        REQUIRE(false);
    }

    if(gtkGreen != magickGreen) {

        REQUIRE(false);
    }

    if(gtkBlue != magickBlue) {

        REQUIRE(false);
    }
}

TEST_CASE("Gdk pixbuf creation works", "[image][gtk][.expensive]")
{
    GetGtkFixture();

    DV::TestDualView DualView;

    auto img =
        DualView.GetCacheManager().LoadFullImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    // Loop while loading //
    while(!img->IsLoaded()) {

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Check that it succeeded //
    REQUIRE(img->IsValid());

    CHECK(img->GetWidth() == 914);
    CHECK(img->GetHeight() == 1280);

    auto gdkImage = img->CreateGtkImage();

    CHECK(gdkImage->get_width() == static_cast<int>(img->GetWidth()));
    CHECK(gdkImage->get_height() == static_cast<int>(img->GetHeight()));

    // Verify pixels //
    Magick::Image& image = img->GetMagickImage()->at(0);

    for(size_t x = 0; x < image.columns(); ++x) {
        for(size_t y = 0; y < image.columns(); ++y) {

            CheckPixel(gdkImage, image, x, y);
        }
    }
}



TEST_CASE("Basic SuperContainer operations", "[components][gtk][.expensive]")
{
    GetGtkFixture();
    DV::DummyDualView dualview;

    Gtk::Window window;

    DV::SuperContainer container;

    window.add(container);
    window.show();

    container.set_size_request(700, 500);

    container.show();

    std::vector<std::shared_ptr<DV::Image>> images;

    for(int i = 0; i < 30; ++i)
        images.push_back(DV::Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg"));

    container.SetShownItems(images.begin(), images.end());

    CHECK(container.GetWidestRowWidth() > 0);
    CHECK(container.CountRows() > 1);

    // TODO: show an actual gtk window for this test to work
    // TODO: find a way to get gtk to initialize the container
    // // 30 images should not be able to fit in 700 pixels
    // CHECK(container.GetWidestRowWidth() <= 700);
}


TEST_CASE(
    "Creating collections and importing image", "[full][integration][.expensive][db][gtk]")
{
    GetGtkFixture();
    boost::filesystem::remove("image_import_test.sqlite");
    DV::TestDualView dualview("image_import_test.sqlite");

    dualview.GetSettings().SetPrivateCollection("non-volatile-test-thumbnails");
    boost::filesystem::create_directories(dualview.GetThumbnailFolder());

    auto img = DV::Image::Create("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    REQUIRE_NOTHROW(dualview.GetDatabase().Init());

    while(!img->IsReady()) {

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(img->IsReady());

    SECTION("Import one image to one collection")
    {
        std::vector<std::shared_ptr<Image>> resources = {img};

        TagCollection tags;

        SECTION("Collection that didn't exist before")
        {
            REQUIRE(dualview.AddToCollection(resources, false, "First collection", tags));

            // Make sure a file was copied //
            CHECK(boost::filesystem::exists(
                boost::filesystem::path(dualview.GetPathToCollection(false)) /
                "collections/First collection/7c2c2141cf27cb90620f80400c6bc3c4.jpg"));
        }
    }
}
