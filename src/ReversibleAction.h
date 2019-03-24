#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace DV {

constexpr auto DEFAULT_UNDO_HISTORY_SIZE = 20;

//! \brief Base class for all action classes that can be undone
class ReversibleAction {
public:
    virtual ~ReversibleAction();
    //! \brief Applies this action again. Or if this hasn't been applied yet also applies this
    //! for the first time
    virtual bool Redo() = 0;

    //! \brief Undoes this action
    virtual bool Undo() = 0;

    virtual bool IsPerformed() const
    {
        return Performed;
    }

protected:
    //! True when this action has been done and can be undone with Undo
    //! This is set to true in Redo
    bool Performed = false;
};

//! \brief Implementation of an undo/redo stack
class ActionHistory {
public:
    ActionHistory(size_t size = DEFAULT_UNDO_HISTORY_SIZE);

    //! \param action The action to add. It must not be performed yet as this will call Redo on
    //! it
    bool AddAction(const std::shared_ptr<ReversibleAction>& action);

    //! \brief Undoes the latest still performed action
    //! \returns True if there was an action to undo and it succeeded, false otherwise.
    bool Undo();

    //! \brief Redoes the latest not performed action
    //! \returns True if there was an action to redo and it succeeded, false otherwise.
    bool Redo();

    //! \brief Returns if undo can be performed. Useful for enabling or disabling buttons
    bool CanUndo() const;

    //! \see CanUndo
    bool CanRedo() const;


protected:
    //! Oldest actions are at the front. New actions are pushed back
    std::deque<std::shared_ptr<ReversibleAction>> Actions;

    size_t ActionsMaxSize;

    //! Undone actions aren't removed from Actions immediately this variable is used to undo /
    //! redo within Actions
    size_t TopOfUndoStack = 0;
};

//! \brief Helper for creating actions with lambdas
class ReversibleActionLambda final : public ReversibleAction {
public:
    ReversibleActionLambda(std::function<void()> redo, std::function<void()> undo) :
        RedoFunction(redo), UndoFunction(undo)
    {}

    bool Redo() override
    {
        if(IsPerformed())
            return false;

        RedoFunction();
        Performed = true;
        return true;
    }

    bool Undo() override
    {
        if(!IsPerformed())
            return false;

        UndoFunction();
        Performed = false;
        return true;
    }

private:
    std::function<void()> RedoFunction;
    std::function<void()> UndoFunction;
};

} // namespace DV
