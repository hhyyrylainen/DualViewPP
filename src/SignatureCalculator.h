#pragma once

#include "Common.h"
#include "IsAlive.h"

#include <atomic>
#include <memory>
#include <vector>

namespace DV {

class Image;

//! \brief Manages calculating signatures for a bunch of images
class SignatureCalculator : public IsAlive {
public:
    SignatureCalculator();
    ~SignatureCalculator();

    void AddImages(const std::vector<DBID>& images);

    void Resume();
    void Pause();

    bool IsDone()
    {
        return Done;
    }

    //! \brief Calculates the signatures for the given image
    //! \returns False on error
    static bool CalculateImageSignature(Image& image);

private:
    //! This is the tail of the queue and contains images that haven't been loaded from the
    //! database yet
    std::vector<DBID> QueueEnd;

    std::vector<std::shared_ptr<Image>> Queue;

    //! Used to throttle if it looks like the database can't keep up
    std::atomic<int> QueuedDatabaseWrites{0};

    bool Done = false;
};

} // namespace DV
