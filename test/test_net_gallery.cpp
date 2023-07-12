#include "catch.hpp"

#include "resources/InternetImage.h"
#include "resources/NetGallery.h"

#include "TestDatabase.h"
#include "TestDualView.h"

using namespace DV;

constexpr auto EXAMPLE_REFERRER = "http://example.com/";
constexpr auto EXAMPLE_URL_1 = "http://example.com/img1.png";
constexpr auto EXAMPLE_URL_2 = "http://example.com/img2.png";
constexpr auto EXAMPLE_URL_3 = "http://example.com/img3.png";
constexpr auto EXAMPLE_URL_4 = "http://example.com/img4.png";

TEST_CASE("NetGallery file insert works", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    GUARD_LOCK_OTHER(db);
    auto gallery = std::make_shared<NetGallery>("example.com", "test gallery");

    db.InsertNetGallery(guard, gallery);

    REQUIRE(gallery->IsInDatabase());
    CHECK(db.SelectNetFilesFromGallery(*gallery).empty());

    const std::vector<std::shared_ptr<InternetImage>> items = {
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_1, std::string(), EXAMPLE_REFERRER)), false),
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_2, std::string(), EXAMPLE_REFERRER)), false)};

    gallery->AddFilesToDownload(items, guard);

    const auto retrieved = db.SelectNetFilesFromGallery(*gallery);

    REQUIRE(retrieved.size() == items.size());
    CHECK(retrieved[0]->GetPageReferrer() == EXAMPLE_REFERRER);
    CHECK(retrieved[1]->GetPageReferrer() == EXAMPLE_REFERRER);

    CHECK(std::count_if(retrieved.begin(), retrieved.end(),
              [](const auto& i) { return i->GetFileURL().GetURL() == EXAMPLE_URL_1; }) == 1);

    CHECK(std::count_if(retrieved.begin(), retrieved.end(),
              [](const auto& i) { return i->GetFileURL().GetURL() == EXAMPLE_URL_2; }) == 1);
}

TEST_CASE("NetGallery file replace works", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    GUARD_LOCK_OTHER(db);
    auto gallery = std::make_shared<NetGallery>("example.com", "test gallery");

    db.InsertNetGallery(guard, gallery);

    REQUIRE(gallery->IsInDatabase());
    CHECK(db.SelectNetFilesFromGallery(*gallery).empty());

    const std::vector<std::shared_ptr<InternetImage>> oldItems = {
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_1, std::string(), EXAMPLE_REFERRER)), false),
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_2, std::string(), EXAMPLE_REFERRER)), false)};

    gallery->AddFilesToDownload(oldItems, guard);

    CHECK(db.SelectNetFilesFromGallery(*gallery).size() == oldItems.size());

    const std::vector<std::shared_ptr<InternetImage>> newItems = {
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_3, std::string(), EXAMPLE_REFERRER)), false),
        InternetImage::Create(ScanFoundImage(ProcessableURL(EXAMPLE_URL_4, std::string(), EXAMPLE_REFERRER)), false)};

    gallery->ReplaceItemsWith(newItems, guard);

    const auto retrieved = db.SelectNetFilesFromGallery(*gallery);

    REQUIRE(retrieved.size() == newItems.size());
    CHECK(retrieved[0]->GetPageReferrer() == EXAMPLE_REFERRER);
    CHECK(retrieved[1]->GetPageReferrer() == EXAMPLE_REFERRER);

    CHECK(std::count_if(retrieved.begin(), retrieved.end(),
              [](const auto& i) { return i->GetFileURL().GetURL() == EXAMPLE_URL_1; }) == 0);

    CHECK(std::count_if(retrieved.begin(), retrieved.end(),
              [](const auto& i) { return i->GetFileURL().GetURL() == EXAMPLE_URL_3; }) == 1);

    CHECK(std::count_if(retrieved.begin(), retrieved.end(),
              [](const auto& i) { return i->GetFileURL().GetURL() == EXAMPLE_URL_4; }) == 1);
}
