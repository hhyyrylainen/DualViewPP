// ------------------------------------ //
#include "ReversibleAction.h"


using namespace DV;
// ------------------------------------ //

// ------------------------------------ //
// ReversibleAction


// ------------------------------------ //
// ActionHistory
ActionHistory::ActionHistory(size_t size /*= DEFAULT_UNDO_HISTORY_SIZE*/) :
    ActionsMaxSize(size)
{}
// ------------------------------------ //
bool ActionHistory::AddAction(const std::shared_ptr<ReversibleAction>& action)
{
    if(!action)
        return false;

    // This may throw so we do this first
    action->Redo();

    // If we aren't at the end of the action stack we need to discard the actions that can't be
    // reached anymore
    while(TopOfUndoStack < Actions.size()) {

        Actions.pop_back();
    }

    Actions.push_back(action);
    ++TopOfUndoStack;

    // Pop old actions if we already have too many items
    while(Actions.size() > ActionsMaxSize) {
        Actions.pop_front();
        --TopOfUndoStack;
    }

    return true;
}
// ------------------------------------ //
bool ActionHistory::Undo()
{
    if(!CanUndo())
        return false;

    // This is allowed to throw
    if(!Actions[TopOfUndoStack - 1]->Undo())
        return false;

    --TopOfUndoStack;
    return true;
}

bool ActionHistory::Redo()
{
    if(!CanRedo())
        return false;

    // This is allowed to throw
    if(!Actions[TopOfUndoStack]->Redo())
        return false;

    ++TopOfUndoStack;
    return true;
}
// ------------------------------------ //
bool ActionHistory::CanUndo() const
{
    return Actions.size() > 0 && TopOfUndoStack > 0;
}

bool ActionHistory::CanRedo() const
{
    return TopOfUndoStack < Actions.size();
}
