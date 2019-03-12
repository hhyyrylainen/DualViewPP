// ------------------------------------ //
#include "SignatureCalculator.h"

#include "resources/Image.h"

#include <Magick++.h>
#include <boost/filesystem.hpp>

extern "C" {
#include "libpuzzle-0.11/src/puzzle.h"
}


using namespace DV;
// ------------------------------------ //

SignatureCalculator::SignatureCalculator() {}
SignatureCalculator::~SignatureCalculator() {}
// ------------------------------------ //
void SignatureCalculator::AddImages(const std::vector<DBID>& images)
{
    Done = false;
}
// ------------------------------------ //
void SignatureCalculator::Resume() {}
void SignatureCalculator::Pause() {}

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

    // TODO: allocating this each time is probably not the most efficient
    PuzzleContext context;
    PuzzleCvec cvec;

    puzzle_init_context(&context);
    puzzle_init_cvec(&context, &cvec);

    bool success = true;

    if(puzzle_fill_cvec_from_memory(&context, &cvec, dataHolder.data(), width, height) == 0) {

        success = true;
        image.SetSignature(std::string(reinterpret_cast<char*>(cvec.vec), cvec.sizeof_vec));

    } else {
        success = false;
    }

    puzzle_free_cvec(&context, &cvec);
    puzzle_free_context(&context);

    return success;
}
