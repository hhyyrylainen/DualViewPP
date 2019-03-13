// ------------------------------------ //
#include "SignatureCalculator.h"

#include "resources/Image.h"

#include "Database.h"
#include "DualView.h"

#include <Magick++.h>
#include <boost/filesystem.hpp>

extern "C" {
#include "libpuzzle-0.11/src/puzzle.h"
}


#include <atomic>
#include <condition_variable>
#include <thread>

using namespace DV;
// ------------------------------------ //
struct SignatureCalculator::Private {

    Private()
    {
        puzzle_init_context(&Context);
    }

    ~Private()
    {
        puzzle_free_context(&Context);
    }

    //! This is the tail of the queue and contains images that haven't been loaded from the
    //! database yet
    std::vector<DBID> QueueEnd;

    std::vector<std::shared_ptr<Image>> Queue;

    //! Used to throttle if it looks like the database can't keep up
    std::atomic<int> QueuedDatabaseWrites{0};

    //! The total number of items added
    int TotalItemsAdded = 0;

    //! The total number of items processed
    std::atomic<int> TotalItemsProcessed{0};

    bool Done = false;

    std::atomic<bool> RunThread = false;

    PuzzleContext Context;
    std::mutex ContextMutex;

    std::mutex DataMutex;
    std::condition_variable WorkerNotify;

    std::thread WorkerThread;
};

// ------------------------------------ //
SignatureCalculator::SignatureCalculator() : pimpl(std::make_unique<Private>()) {}
SignatureCalculator::~SignatureCalculator()
{
    Pause();

    if(pimpl->WorkerThread.joinable())
        pimpl->WorkerThread.join();
}
// ------------------------------------ //
void SignatureCalculator::AddImages(const std::vector<DBID>& images)
{
    pimpl->Done = false;

    std::lock_guard<std::mutex> guard(pimpl->DataMutex);

    pimpl->QueueEnd.reserve(pimpl->QueueEnd.size() + images.size());
    pimpl->QueueEnd.insert(pimpl->QueueEnd.end(), images.begin(), images.end());

    pimpl->WorkerNotify.notify_all();

    LOG_INFO(
        "SignatureCalculator: queue size is now: " + std::to_string(pimpl->QueueEnd.size()) +
        ", loaded images: " + std::to_string(pimpl->Queue.size()));
}

void SignatureCalculator::AddImages(const std::vector<std::shared_ptr<Image>>& images)
{
    pimpl->Done = false;

    std::lock_guard<std::mutex> guard(pimpl->DataMutex);

    pimpl->Queue.reserve(pimpl->Queue.size() + images.size());
    pimpl->Queue.insert(pimpl->Queue.end(), images.begin(), images.end());

    pimpl->WorkerNotify.notify_all();

    LOG_INFO(
        "SignatureCalculator: queue size is now: " + std::to_string(pimpl->QueueEnd.size()) +
        ", loaded images: " + std::to_string(pimpl->Queue.size()));
}
// ------------------------------------ //
void SignatureCalculator::Resume()
{
    if(!pimpl->RunThread) {
        pimpl->RunThread = true;

        if(pimpl->WorkerThread.joinable())
            pimpl->WorkerThread.join();

        pimpl->WorkerThread = std::thread(&SignatureCalculator::_RunCalculationThread, this);
    }
}

void SignatureCalculator::Pause()
{
    pimpl->RunThread = false;

    std::lock_guard<std::mutex> guard(pimpl->DataMutex);
    pimpl->WorkerNotify.notify_all();
}

bool SignatureCalculator::IsDone() const
{
    return pimpl->Done;
}
// ------------------------------------ //
void SignatureCalculator::_RunCalculationThread()
{
    LOG_INFO("SignatureCalculator: running worker thread");

    std::unique_lock<std::mutex> lock(pimpl->DataMutex);

    bool didSomethingOld = false;

    while(pimpl->RunThread) {

        bool somethingToDo = false;

        // Queue DB read if too few items are loaded (and there are items to load)
        if(pimpl->Queue.size() <= SIGNATURE_CALCULATOR_READ_MORE_THRESSHOLD &&
            !pimpl->QueueEnd.empty()) {

            somethingToDo = true;
            didSomethingOld = true;

            std::vector<DBID> itemsToRead;
            itemsToRead.reserve(SIGNATURE_CALCULATOR_READ_BATCH);

            while(!pimpl->QueueEnd.empty() &&
                  itemsToRead.size() < SIGNATURE_CALCULATOR_READ_BATCH) {

                itemsToRead.push_back(pimpl->QueueEnd.back());
                pimpl->QueueEnd.pop_back();
            }

            auto isalive = GetAliveMarker();

            DualView::Get().QueueDBThreadFunction([=]() {
                std::vector<std::shared_ptr<Image>> images;
                images.reserve(itemsToRead.size());

                for(auto item : itemsToRead) {
                    auto image = DualView::Get().GetDatabase().SelectImageByIDAG(item);

                    if(image)
                        images.push_back(image);
                }

                LOG_INFO("SignatureCalculator: loaded images from DB: " +
                         std::to_string(images.size()));

                DualView::Get().InvokeFunction([this, isalive, images]() {
                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    AddImages(images);
                });
            });
        }

        // Process items
        if(!pimpl->Queue.empty()) {

            somethingToDo = true;
            didSomethingOld = true;

            auto image = pimpl->Queue.back();
            pimpl->Queue.pop_back();

            // Unlock while processing an item
            lock.unlock();

            if(image) {
                // Calculate new signature
                CalculateImageSignature(*image);

                // Save the updated signature
                if(image->IsInDatabase())
                    image->Save();
                LOG_INFO("SignatureCalculator: calculated for image: " + image->GetName());
            }

            lock.lock();
        }

        if(!somethingToDo) {
            // Nothing to do

            // If also didn't anything last time this is now done
            if(!didSomethingOld) {
                pimpl->Done = true;
                LOG_INFO("SignatureCalculator: has finished with all work");
            }

            didSomethingOld = false;

            // Sleep while waiting for something to happen
            pimpl->WorkerNotify.wait_for(lock, std::chrono::seconds(30));
        }
    }

    LOG_INFO("SignatureCalculator: running worker thread exiting");
}
// ------------------------------------ //
bool SignatureCalculator::CalculateImageSignature(Image& image)
{
    const auto file = image.GetResourcePath();

    if(!boost::filesystem::exists(file))
        return false;

    std::vector<Magick::Image> createdImage;

    // Load image //
    try {
        readImages(&createdImage, file);

    } catch(const Magick::Error& e) {

        // Loading failed //
        return false;
    }

    if(createdImage.empty())
        return false;

    // Coalesce animated images //
    if(createdImage.size() > 1) {

        std::vector<Magick::Image> image;
        coalesceImages(&image, createdImage.begin(), createdImage.end());

        if(image.empty())
            return false;

        createdImage = image;
    }

    const auto width = createdImage.front().columns();
    const auto height = createdImage.front().rows();

    std::vector<char> dataHolder;
    dataHolder.resize(3 * width * height);

    createdImage.front().write(
        0, 0, width, height, "RGB", Magick::CharPixel, dataHolder.data());

    // Lock for the puzzle context
    std::lock_guard<std::mutex> lock(pimpl->ContextMutex);

    PuzzleCvec cvec;
    puzzle_init_cvec(&pimpl->Context, &cvec);

    bool success = true;

    if(puzzle_fill_cvec_from_memory(
           &pimpl->Context, &cvec, dataHolder.data(), width, height) == 0) {

        success = true;
        // TODO: this DB write could be outside the mutex
        image.SetSignature(std::string(reinterpret_cast<char*>(cvec.vec), cvec.sizeof_vec));

    } else {
        success = false;
    }

    puzzle_free_cvec(&pimpl->Context, &cvec);
    return success;
}
