#include "catch.hpp"

#include "ReversibleAction.h"


using namespace DV;

TEST_CASE("Undo stack handling", "[undo]")
{
    auto action1 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action2 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action3 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action4 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action5 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action6 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});
    auto action7 = std::make_shared<ReversibleActionLambda>([]() {}, []() {});

    ActionHistory history(10);

    SECTION("Basic linear stack add, then undo and then redos")
    {
        // Add all except the last
        history.AddAction(action1);
        history.AddAction(action2);
        history.AddAction(action3);
        history.AddAction(action4);
        history.AddAction(action5);
        history.AddAction(action6);

        CHECK(action1->IsPerformed());
        CHECK(action2->IsPerformed());
        CHECK(action3->IsPerformed());
        CHECK(action4->IsPerformed());
        CHECK(action5->IsPerformed());
        CHECK(action6->IsPerformed());

        CHECK(!action7->IsPerformed());

        CHECK(history.CanUndo());
        CHECK(!history.CanRedo());

        // Undo one by one
        CHECK(history.Undo());
        CHECK(!action6->IsPerformed());
        CHECK(action5->IsPerformed());

        CHECK(history.CanRedo());

        CHECK(history.Undo());
        CHECK(!action5->IsPerformed());
        CHECK(action4->IsPerformed());

        CHECK(history.Undo());
        CHECK(!action4->IsPerformed());
        CHECK(action3->IsPerformed());

        CHECK(history.Undo());
        CHECK(!action3->IsPerformed());
        CHECK(action2->IsPerformed());

        CHECK(history.Undo());
        CHECK(!action2->IsPerformed());

        CHECK(history.Undo());
        CHECK(!action1->IsPerformed());

        CHECK(!history.CanUndo());
        CHECK(history.CanRedo());

        CHECK(history.Redo());
        CHECK(action1->IsPerformed());
        CHECK(!action2->IsPerformed());

        CHECK(history.Redo());
        CHECK(action2->IsPerformed());
    }

    SECTION("Adding new actions while having undone actions")
    {
        history.AddAction(action1);
        history.AddAction(action2);

        CHECK(history.Undo());
        CHECK(!action2->IsPerformed());

        history.AddAction(action3);
        history.AddAction(action4);

        CHECK(!action2->IsPerformed());
        CHECK(action3->IsPerformed());
        CHECK(action4->IsPerformed());

        CHECK(!history.Redo());
        CHECK(history.Undo());
        CHECK(!action4->IsPerformed());

        CHECK(history.Redo());
        CHECK(action4->IsPerformed());
    }
}
