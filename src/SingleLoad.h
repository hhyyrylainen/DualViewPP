#pragma once

#include <map>
#include <memory>

namespace DV {

//! \brief Helper for only allowing a single instance to be loaded with the specific ID
template<class TObj, typename IDType>
class SingleLoad {
public:
    //! \brief Call when an object has been created to add it to the cache
    //! \note The object will be replaced with one from the cache if such object exists
    void OnLoad(std::shared_ptr<TObj>& object)
    {
        const auto id = object->GetID();
        auto iter = LoadedObjects.find(id);

        if(iter == LoadedObjects.end()) {

            // New item //
            LoadedObjects[id] = object;
            return;
        }

        // There's potentially an existing one //
        auto existing = iter->second.lock();

        if(!existing) {

            // It was expired //
            iter->second = object;
            return;
        }

        // Should be fine //
        object = existing;
    }

    std::shared_ptr<TObj> GetIfLoaded(const IDType id)
    {
        auto iter = LoadedObjects.find(id);

        if(iter == LoadedObjects.end())
            return nullptr;

        return iter->second.lock();
    }

    //! \brief Cleans up expired objects
    void Purge()
    {
        for(auto iter = LoadedObjects.begin(); iter != LoadedObjects.end();) {

            if(iter->second.expired()) {

                // Erase expired item //
                iter = LoadedObjects.erase(iter);
            } else {

                ++iter;
            }
        }
    }

    //! \brief Removes an entry
    //!
    //! Use this when permanently deleting something
    void Remove(const IDType id)
    {
        auto iter = LoadedObjects.find(id);

        if(iter == LoadedObjects.end())
            return;

        LoadedObjects.erase(iter);
    }

protected:
    std::map<IDType, std::weak_ptr<TObj>> LoadedObjects;
};

} // namespace DV
