#pragma once

#include "TimeHelpers.h"

#include "Common/ThreadSafe.h"

#include <deque>
#include <memory>
#include <mutex>

namespace DV {

using PriorityValueT = int64_t;

//! \brief Base class for TaskItem that can be used when the actual task is unnecessary to know
//! \todo Add a cancel interface here?
class BaseTaskItem {
protected:
    explicit BaseTaskItem(PriorityValueT priority) noexcept : Priority(priority) {}

public:
    //! \brief Bumps this to the front of the task queue
    void Bump()
    {
        SetPriority(TimeHelpers::GetCurrentUnixTimestamp());
    }

    void SetPriority(PriorityValueT newPriority)
    {
        Priority = newPriority;
    }

    [[nodiscard]] auto GetPriority() const
    {
        return Priority.load(std::memory_order_acquire);
    }

    void OnDone()
    {
        Done = true;
    }

private:
    std::atomic<bool> Done{false};
    std::atomic<PriorityValueT> Priority;
};

//! \brief Keeps a list of tasks that can be executed, and priority changed while running
//!
//! For performance this only performs partial sorts when getting the task to execute
template<class T>
class TaskListWithPriority : public ThreadSafe {
public:
    //! \brief A queued task. This instance can be used to increase the priority of tasks
    class TaskItem : public BaseTaskItem {
    public:
        TaskItem(T& item, PriorityValueT priority) noexcept :
            BaseTaskItem(priority), Task(std::move(item))
        {}

    public:
        const T Task;
    };

public:
    //! \brief Adds a new task to be ran
    std::shared_ptr<TaskItem> Push(
        Lock& guard, T item, PriorityValueT priority = TimeHelpers::GetCurrentUnixTimestamp())
    {
        const auto task = std::make_shared<TaskItem>(item, priority);

        Queue.emplace_back(task);
        return task;
    }

    void Clear()
    {
        GUARD_LOCK();
        Clear(guard);
    }

    void Clear(Lock& guard)
    {
        Queue.clear();
        SinceLastFullSort = 0;
    }

    bool Empty(Lock& guard) const
    {
        return Queue.empty();
    }

    //! \brief Gets the next task to run and removes it from the queue
    std::shared_ptr<TaskItem> Pop(Lock& guard)
    {
        if(Queue.empty())
            return nullptr;

        ++SinceLastFullSort;
        ++SinceFrontProcess;

        // A bit of a hack to make thumbnail and gallery loading nicer looking at the front
        if(SinceFrontProcess >= FrontProcessInterval) {

            auto result = Queue.front();
            Queue.pop_front();

            SinceFrontProcess = 0;
            return result;
        }

        const bool fullLook = SinceLastFullSort >= FullSortInterval;
        SinceLastFullSort = 0;

        // Find kind of the highest priority as well as do some bubble sorting

        int timeLooking = 20;

        auto bestTask = Queue.rbegin();

        for(auto iter = bestTask; iter != Queue.rend();) {
            // Can't do anything really with the last item
            const auto next = std::next(iter);
            if(next == Queue.rend())
                break;

            const auto currentPriority = (*iter)->GetPriority();
            const auto nextPriority = (*next)->GetPriority();
            const auto bestPriority = (*bestTask)->GetPriority();

            if(currentPriority < nextPriority) {

                std::swap(*iter, *next);

                if(nextPriority >= bestPriority) {
                    bestTask = iter;
                }
            } else {
                if(currentPriority >= bestPriority) {
                    bestTask = iter;
                }
            }

            if(currentPriority != nextPriority)
                --timeLooking;

            if(timeLooking < 0 && !fullLook) {
                break;
            }

            ++iter;
        }

        if(bestTask != Queue.rend()) {
            const auto result = *bestTask;
            Queue.erase(std::next(bestTask).base());
            return result;
        }

        // Fallback for just returning the last item
        if(!Queue.empty()) {
            const auto result = Queue.back();
            Queue.pop_back();
            return result;
        }

        return nullptr;
    }

private:
    const int FullSortInterval = 20;
    const int FrontProcessInterval = 5;

    std::deque<std::shared_ptr<TaskItem>> Queue;
    int SinceLastFullSort = 0;
    int SinceFrontProcess = 0;
};
} // namespace DV
