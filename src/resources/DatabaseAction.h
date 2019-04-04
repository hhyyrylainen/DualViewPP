#pragma once

#include "Common.h"
#include "DatabaseResource.h"
#include "ReversibleAction.h"

namespace Json {
class Value;
}

namespace DV {

class Database;
class PreparedStatement;

//! \brief Used to create the correct class from the action_history table
enum class DATABASE_ACTION_TYPE : int {
    ImageDelete = 1,
    Invalid /* must be always last value */
};

//! \brief All reversible database operations result in this that can then be used to reverse
//! it
//!
//! The action data is stored as JSON in the database and this class doesn't really understand
//! what this contains. The Database is given the JSON data for all processing
class DatabaseAction : public ReversibleAction, public DatabaseResource {
    friend Database;

protected:
    DatabaseAction();
    DatabaseAction(DBID id, Database& from);

public:
    virtual ~DatabaseAction();

    static std::shared_ptr<DatabaseAction> Create(
        Database& db, Lock& dblock, PreparedStatement& statement, DBID id);

    bool Redo() override;
    bool Undo() override;

    virtual DATABASE_ACTION_TYPE GetType() const = 0;

    virtual std::string SerializeData() const;

    bool IsDeleted() const
    {
        return Deleted;
    }

protected:
    //! Called when removed from the database. This will permanently delete any resources still
    //! held by this action. For example Image is first marked deleted and once the action is
    //! popped off the history then the Image is permanently deleted
    //! \note This needs to be called from derived classes to set Deleted
    virtual void _OnPurged() override;

    virtual void _SerializeCustomData(Json::Value& value) const = 0;

    void _ReportPerformedStatus(bool performed);

private:
    virtual void _Redo() = 0;
    virtual void _Undo() = 0;

    void _DoSave(Database& db) override;
    void _DoSave(Database& db, Lock& dblock) override;

private:
    bool Deleted = false;
};

//! \brief Image was deleted (marked deleted)
class ImageDeleteAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, Lock& dblock, PreparedStatement& statement, DBID id);

protected:
    ImageDeleteAction(DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    ImageDeleteAction(const std::vector<DBID>& images);
    ~ImageDeleteAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::ImageDelete;
    }

    const auto& GetImagesToDelete() const
    {
        return ImagesToDelete;
    }

protected:
    void SetImagesToDelete(const std::vector<DBID>& images);

    void _OnPurged() override;

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    std::vector<DBID> ImagesToDelete;
};

} // namespace DV
