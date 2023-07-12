// ------------------------------------ //
#include "PluginManager.h"

#include <fstream>

#ifdef __linux__
#include <dlfcn.h>
#else
#error todo: shared object opening
#endif

using namespace DV;

// ------------------------------------ //
PluginManager::PluginManager() = default;

PluginManager::~PluginManager()
{
    // Unload all //
    // They are actually smart pointers, so it might not unload all
    WebsiteScanners.clear();

    // Try to unload libraries //
    for (void* handle : OpenLibraryHandles)
    {
        dlclose(handle);
    }

    OpenLibraryHandles.clear();
}

// ------------------------------------ //
bool PluginManager::LoadPlugin(const std::string& fileName)
{
    // Make sure the file exists //
    {
        std::ifstream file(fileName);

        if (!file.good())
        {
            LOG_ERROR("Plugin file doesn't exist: " + fileName);
            return false;
        }
    }

    LOG_INFO("Loading plugin file: " + fileName);

    void* soHandle = dlopen(fileName.c_str(), RTLD_NOW);

    if (!soHandle)
    {
        auto* errorStr = dlerror();

        LOG_ERROR(
            "Failed to load plugin library '" + fileName + "': " + (errorStr ? std::string(errorStr) : std::string()));

        return false;
    }

    OpenLibraryHandles.push_back(soHandle);

    CreateDescriptionFuncPtr creation;
    DestroyDescriptionFuncPtr deletion;

    // Lookup functions //

    // Clear errors, just for good practice
    dlerror();

    creation = reinterpret_cast<CreateDescriptionFuncPtr>(dlsym(soHandle, "CreatePluginDesc"));

    char* error = dlerror();
    if (error || !creation)
    {
        LOG_ERROR("Required function (create plugin info) not found in plugin '" + fileName +
            "': " + (error ? std::string(error) : std::string()));
        return false;
    }

    deletion = reinterpret_cast<DestroyDescriptionFuncPtr>(dlsym(soHandle, "DestroyPluginDesc"));

    error = dlerror();
    if (error || !deletion)
    {
        LOG_ERROR("Required function (delete plugin info) not found in plugin '" + fileName +
            "': " + (error ? std::string(error) : std::string()));
        return false;
    }

    std::shared_ptr<IPluginDescription> pluginDesc(creation(), [=](IPluginDescription* obj) { deletion(obj); });

    if (!pluginDesc)
    {
        LOG_ERROR("PluginDescription retrieval failed for: " + fileName);
        return false;
    }

    // Sanity check
    {
        std::string sanityStr = pluginDesc->GetTheAnswer();

        if (sanityStr != std::string("42"))
        {
            LOG_ERROR("Plugin sanity check failed for: " + fileName);
            return false;
        }
    }
    // The string destructor might also break everything so maybe install a signal handler
    // that is active while the sanity check is done

    // Verify version //
    std::string pluginVersion = pluginDesc->GetDualViewVersionStr();

    if (pluginVersion != DUALVIEW_VERSION)
    {
        LOG_ERROR("Plugin version mismatch: in '" + fileName + "': plugin version: " + pluginVersion +
            " required version: " + DUALVIEW_VERSION);
        return false;
    }

    // Get downloaders //
    {
        for (const auto& scanner : pluginDesc->GetSupportedSites())
            AddScanner(scanner);
    }

    LOG_INFO(
        "Plugin: " + std::string(pluginDesc->GetPluginName()) + " successfully loaded (" + pluginDesc->GetUUID() + ")");

    return true;
}

// ------------------------------------ //
void PluginManager::AddScanner(const std::shared_ptr<IWebsiteScanner>& scanner)
{
    for (const auto& existing : WebsiteScanners)
    {
        if (existing->GetName() == scanner->GetName())
            return;
    }

    LOG_INFO("PluginManager: loaded new download plugin: " + std::string(scanner->GetName()));
    WebsiteScanners.push_back(scanner);
}

std::shared_ptr<IWebsiteScanner> PluginManager::GetScannerForURL(const std::string& url) const
{
    for (const auto& scanner : WebsiteScanners)
    {
        if (scanner->CanHandleURL(url))
            return scanner;
    }

    return nullptr;
}

// ------------------------------------ //
void PluginManager::PrintPluginStats() const
{
    LOG_INFO("PluginManager has loaded:");

    LOG_WRITE(Convert::ToString(WebsiteScanners.size()) + " website scan plugins:");

    for (const auto& dl : WebsiteScanners)
        LOG_WRITE("- " + std::string(dl->GetName()));

    LOG_WRITE("");
    LOG_WRITE("");
}
