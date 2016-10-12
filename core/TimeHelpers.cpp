// ------------------------------------ //
#include "TimeHelpers.h"

using namespace DV;
// ------------------------------------ //

std::atomic<bool> TimeHelpers::IsInitialized = { false };
std::mutex TimeHelpers::InitializeMutex;
