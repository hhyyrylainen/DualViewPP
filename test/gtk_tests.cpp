#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "core/CacheManager.h"

#include <Magick++.h>

#include <thread>
#include <memory>

#include <iostream>


//! \file All tests that require gtk to be initialized

class GtkTestsFixture {
public:
    
    GtkTestsFixture() :
        log(std::make_unique<Leviathan::TestLogger>("gtk_tests.txt"))
    {
        app = Gtk::Application::create("com.boostslair.dualview.tests");

        REQUIRE(app->register_application());
    }


protected:

    Glib::RefPtr<Gtk::Application> app;
    
    std::unique_ptr<Leviathan::TestLogger> log;
    
    DV::TestDualView DualView;
};

//#define PRINT_PIXEL_VALUES

void CheckPixel(Glib::RefPtr<Gdk::Pixbuf> &pixbuf, Magick::Image &image, size_t x, size_t y){

    const auto &magickColour = image.pixelColor(x, y);
    auto* pixels = pixbuf->get_pixels();
    const auto gtkRed = pixels[(x * 3) + (y * pixbuf->get_rowstride())];
    const auto gtkGreen = pixels[(x * 3) + (y * pixbuf->get_rowstride()) + 1];
    const auto gtkBlue = pixels[(x * 3) + (y * pixbuf->get_rowstride()) + 2];

    const auto magickRed = MagickCore::ScaleQuantumToChar(magickColour.redQuantum());
    const auto magickGreen = MagickCore::ScaleQuantumToChar(magickColour.greenQuantum());
    const auto magickBlue = MagickCore::ScaleQuantumToChar(magickColour.blueQuantum());

#ifdef PRINT_PIXEL_VALUES
    std::cout << "Comparing: " << (int)magickRed << ", "
        << (int)magickGreen << ", "
        << (int)magickBlue << std::endl;

    std::cout << "With     : " << (int)gtkRed << ", "
        << (int)gtkGreen << ", "
        << (int)gtkBlue << std::endl;
#endif
    
    if(gtkRed != magickRed){

        REQUIRE(false);
    }

    if(gtkGreen != magickGreen){

        REQUIRE(false);
    }

    if(gtkBlue != magickBlue){

        REQUIRE(false);
    }
}

TEST_CASE_METHOD(GtkTestsFixture, "Gdk pixbuf creation works", "[image, gtk]") {

    auto img = DualView.GetCacheManager().LoadFullImage(
        "data/7c2c2141cf27cb90620f80400c6bc3c4.jpg");

    REQUIRE(img);

    // Loop while loading //
    while(!img->IsLoaded()){

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Check that it succeeded //
    REQUIRE(img->IsValid());

    CHECK(img->GetWidth() == 914);
    CHECK(img->GetHeight() == 1280);

    auto gdkImage = img->CreateGtkImage();

    CHECK(gdkImage->get_width() == img->GetWidth());
    CHECK(gdkImage->get_height() == img->GetHeight());

    // Verify pixels //
    Magick::Image& image = img->GetMagickImage()->at(0);

    for(size_t x = 0; x < image.columns(); ++x){
        for(size_t y = 0; y < image.columns(); ++y){

            CheckPixel(gdkImage, image, x, y);
        }
    }
    
}



