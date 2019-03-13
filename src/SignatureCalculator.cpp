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
    std::atomic<int> TotalItemsAdded{0};

    //! The total number of items processed
    std::atomic<int> TotalItemsProcessed{0};

    bool Done = false;

    //! Used to only read one batch of images at a time
    std::atomic<bool> DBReadInProgress = false;

    //! Used to have one save operation going on at once
    std::shared_ptr<std::atomic<bool>> DBWriteInProgress =
        std::make_shared<std::atomic<bool>>(false);

    std::atomic<bool> RunThread = false;

    PuzzleContext Context;
    std::mutex ContextMutex;

    std::mutex DataMutex;
    std::condition_variable WorkerNotify;

    std::thread WorkerThread;

    std::function<void(int processed, int total, bool done)> Callback;
};

// ------------------------------------ //
SignatureCalculator::SignatureCalculator() : pimpl(std::make_unique<Private>()) {}
SignatureCalculator::~SignatureCalculator()
{
    Pause(true);
}
// ------------------------------------ //
void SignatureCalculator::AddImages(const std::vector<DBID>& images)
{
    pimpl->Done = false;

    std::lock_guard<std::mutex> guard(pimpl->DataMutex);

    pimpl->TotalItemsAdded += images.size();

    pimpl->QueueEnd.reserve(pimpl->QueueEnd.size() + images.size());
    pimpl->QueueEnd.insert(pimpl->QueueEnd.end(), images.begin(), images.end());

    pimpl->WorkerNotify.notify_all();

    LOG_INFO(
        "SignatureCalculator: queue size is now: " + std::to_string(pimpl->QueueEnd.size()) +
        ", loaded images: " + std::to_string(pimpl->Queue.size()));

    _ReportStatus();
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

void SignatureCalculator::Pause(bool wait /*= false*/)
{
    pimpl->RunThread = false;

    {
        std::lock_guard<std::mutex> guard(pimpl->DataMutex);
        pimpl->WorkerNotify.notify_all();
    }

    if(wait) {
        if(pimpl->WorkerThread.joinable())
            pimpl->WorkerThread.join();
    }
}

bool SignatureCalculator::IsDone() const
{
    return pimpl->Done;
}
// ------------------------------------ //
void SignatureCalculator::SetStatusListener(
    std::function<void(int processed, int total, bool done)> callback)
{
    pimpl->Callback = callback;
}

void SignatureCalculator::_ReportStatus() const
{
    if(pimpl->Callback) {
        pimpl->Callback(pimpl->TotalItemsProcessed, pimpl->TotalItemsAdded, pimpl->Done);
    }
}
// ------------------------------------ //
void _QueueToDB(const std::vector<std::shared_ptr<Image>>& savequeue)
{
    auto& db = DualView::Get().GetDatabase();

    GUARD_LOCK_OTHER(db);

    DoDBTransaction transaction(db, guard, true);

    for(auto& item : savequeue)
        item->Save(db, guard);
}

void SignatureCalculator::_SaveQueueHelper(
    std::vector<std::shared_ptr<Image>>& savequeue, bool runinbackground)
{
    if(savequeue.empty())
        return;

    if(runinbackground) {

        // Only one at a time
        while(*pimpl->DBWriteInProgress)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // No race condition here as only one thread queues these
        *pimpl->DBWriteInProgress = true;

        auto isalive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([status{pimpl->DBWriteInProgress}, savequeue]() {
            if(savequeue.empty())
                LOG_ERROR("SignatureCalculator: save queue became empty in transit");

            _QueueToDB(savequeue);

            *status = false;
        });

    } else {

        _QueueToDB(savequeue);
    }

    savequeue.clear();
}

void SignatureCalculator::_RunCalculationThread()
{
    LOG_INFO("SignatureCalculator: running worker thread");

    std::vector<std::shared_ptr<Image>> saveQueue;

    if(SIGNATURE_CALCULATOR_GROUP_IMAGE_SAVE > 1)
        saveQueue.reserve(SIGNATURE_CALCULATOR_GROUP_IMAGE_SAVE);

    std::unique_lock<std::mutex> lock(pimpl->DataMutex);

    bool didSomethingOld = false;

    while(pimpl->RunThread) {

        bool somethingToDo = false;

        // Queue DB read if too few items are loaded (and there are items to load)
        if(pimpl->Queue.size() <= SIGNATURE_CALCULATOR_READ_MORE_THRESSHOLD &&
            !pimpl->QueueEnd.empty() && !pimpl->DBReadInProgress) {

            pimpl->DBReadInProgress = true;

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

                DualView::Get().InvokeFunction([this, isalive, images]() {
                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    AddImages(images);
                    pimpl->DBReadInProgress = false;
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
                if(image->IsInDatabase()) {

                    if(SIGNATURE_CALCULATOR_GROUP_IMAGE_SAVE < 2) {
                        image->Save();
                    } else {
                        saveQueue.push_back(image);

                        if(saveQueue.size() > SIGNATURE_CALCULATOR_GROUP_IMAGE_SAVE)
                            _SaveQueueHelper(saveQueue, true);
                    }
                }

                // A lot of spam
                // LOG_INFO("SignatureCalculator: calculated for image: " + image->GetName());
            }

            ++pimpl->TotalItemsProcessed;
            _ReportStatus();
            lock.lock();
        }

        if(!somethingToDo) {
            // Nothing to do
            // Save queue if it has something
            if(!saveQueue.empty()) {
                lock.unlock();
                _SaveQueueHelper(saveQueue, true);
                lock.lock();
            }

            // If also didn't anything last time this is now done. And isn't waiting for a
            // database read then this is done
            if(!didSomethingOld && !pimpl->Done && !pimpl->DBReadInProgress) {
                pimpl->Done = true;
                _ReportStatus();
                LOG_INFO("SignatureCalculator: has finished with all work");
            }

            didSomethingOld = false;

            // Sleep while waiting for something to happen
            pimpl->WorkerNotify.wait_for(lock, std::chrono::seconds(5));
        }
    }

    _SaveQueueHelper(saveQueue, false);

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
