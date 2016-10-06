// ------------------------------------ //
#include "CurlWrapper.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

CurlWrapper::CurlWrapper(){

    CheckCurlInit();

    WrappedObject = curl_easy_init();

    LEVIATHAN_ASSERT(WrappedObject, "cURL initialization failed");
}

CurlWrapper::~CurlWrapper(){

    curl_easy_cleanup(WrappedObject); 
    WrappedObject = nullptr;
}
// ------------------------------------ //
std::mutex CurlWrapper::CurlInitMutex;
bool CurlWrapper::CurlInitialized;

void CurlWrapper::CheckCurlInit(){

    std::lock_guard<std::mutex> lock(CurlInitMutex);

    if(CurlInitialized)
        return;

    curl_global_init(CURL_GLOBAL_ALL);

    CurlInitialized = true;
}


