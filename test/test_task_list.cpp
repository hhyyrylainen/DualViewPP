#include "catch.hpp"

#include "TaskListWithPriority.h"

using namespace DV;

struct DummyTask {
public:
    //! \brief Makes new dummy task
    //! \param id The unique id for this must be unique within a task list
    explicit DummyTask(int id) : ID(id) {}

    DummyTask(const DummyTask& other) = default;
    DummyTask& operator=(const DummyTask& other) = delete;

    bool operator==(const DummyTask& other) const
    {
        return ID == other.ID;
    }

    bool operator!=(const DummyTask& other) const
    {
        return !(*this == other);
    }

    const int ID;
};

std::ostream& operator<<(std::ostream& os, const DummyTask& t)
{
    os << t.ID;
    return os;
}

TEST_CASE("Basic task queue insert and pop works", "[task]")
{
    int priority = 1;

    DummyTask task1{1};
    DummyTask task2{2};
    DummyTask task3{3};
    DummyTask task4{4};
    DummyTask task5{5};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

    CHECK(list.Empty(guard));

    list.Push(guard, task1, priority++);

    CHECK(!list.Empty(guard));

    list.Push(guard, task2, priority++);
    list.Push(guard, task3, priority++);
    list.Push(guard, task4, priority++);
    list.Push(guard, task5, priority++);

    CHECK(list.Pop(guard)->Task == task5);
    CHECK(list.Pop(guard)->Task == task4);
    CHECK(list.Pop(guard)->Task == task3);
    CHECK(list.Pop(guard)->Task == task2);
    CHECK(list.Pop(guard)->Task == task1);

    CHECK(list.Empty(guard));
}

TEST_CASE("Task queue clear works", "[task]")
{
    int priority = 1;

    DummyTask task1{1};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

    CHECK(list.Empty(guard));

    list.Push(guard, task1, priority++);

    CHECK(!list.Empty(guard));

    list.Clear(guard);

    CHECK(list.Empty(guard));
    CHECK(list.Pop(guard) == nullptr);
}

TEST_CASE("Second last task is higher priority", "[task]")
{
    DummyTask task1{1};
    DummyTask task2{2};
    DummyTask task3{3};
    DummyTask task4{4};
    DummyTask task5{5};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

    list.Push(guard, task1, 1);

    CHECK(!list.Empty(guard));

    list.Push(guard, task2, 2);
    list.Push(guard, task3, 3);
    list.Push(guard, task4, 5);
    list.Push(guard, task5, 4);

    const auto first = list.Pop(guard);

    CHECK(first->Task != task5);
    CHECK(first->Task == task4);

    CHECK(list.Pop(guard)->Task == task5);
    CHECK(list.Pop(guard)->Task == task3);
    CHECK(list.Pop(guard)->Task == task2);
    CHECK(list.Pop(guard)->Task == task1);

    CHECK(list.Pop(guard) == nullptr);
}

TEST_CASE("Task queue priorities work", "[task]")
{
    DummyTask task1{1};
    DummyTask task2{2};
    DummyTask task3{3};
    DummyTask task4{4};
    DummyTask task5{5};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

    list.Push(guard, task1, 1);
    list.Push(guard, task2, 15);
    list.Push(guard, task3, 2);
    list.Push(guard, task4, 4);
    list.Push(guard, task5, 3);

    const auto first = list.Pop(guard);

    CHECK(first->Task != task5);
    CHECK(first->Task == task2);

    CHECK(list.Pop(guard)->Task == task4);
    CHECK(list.Pop(guard)->Task == task5);
    CHECK(list.Pop(guard)->Task == task3);
    CHECK(list.Pop(guard)->Task == task1);
}

TEST_CASE("Task queue priorities can change while running", "[task]")
{
    DummyTask task1{1};
    DummyTask task2{2};
    DummyTask task3{3};
    DummyTask task4{4};
    DummyTask task5{5};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

    const auto first = list.Push(guard, task1, 1);
    list.Push(guard, task2, 2);
    const auto third = list.Push(guard, task3, 3);
    list.Push(guard, task4, 4);
    list.Push(guard, task5, 5);

    CHECK(list.Pop(guard)->Task == task5);

    third->SetPriority(8);

    CHECK(list.Pop(guard)->Task == task3);
    CHECK(list.Pop(guard)->Task == task4);

    first->SetPriority(19);

    CHECK(list.Pop(guard)->Task == task1);
    CHECK(list.Pop(guard)->Task == task2);
}

TEST_CASE("Same priority tasks come out in insertion order", "[task]")
{
    DummyTask task1{1};
    DummyTask task2{2};
    DummyTask task3{3};
    DummyTask task4{4};
    DummyTask task5{5};

    TaskListWithPriority<DummyTask> list;
    GUARD_LOCK_OTHER(list);

   list.Push(guard, task1, 1);
    list.Push(guard, task2, 2);
    list.Push(guard, task3, 2);
    list.Push(guard, task4, 2);
    list.Push(guard, task5, 3);

    CHECK(list.Pop(guard)->Task == task5);
    CHECK(list.Pop(guard)->Task == task2);
    CHECK(list.Pop(guard)->Task == task3);
    CHECK(list.Pop(guard)->Task == task4);
    CHECK(list.Pop(guard)->Task == task1);
}
