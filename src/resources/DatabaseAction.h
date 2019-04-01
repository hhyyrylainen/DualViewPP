#pragma once

#include "Common.h"
#include "DatabaseResource.h"
#include "ReversibleAction.h"

namespace Json {
class Value;
}

namespace DV {

class Database;

//! \brief Used to create the correct class from the action_history table
enum class DATABASE_ACTION_TYPE : int { ImageDelete = 1 };

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

    bool Redo() override;
    bool Undo() override;

    //! \brief Removes all resources needed by this action
    //!
    //! Called when removed from the database. This will permanently delete any resources still
    //! held by this action. For example Image is first marked deleted and once the action is
    //! popped off the history then the Image is permanently deleted
    virtual void Purge() = 0;

    virtual DATABASE_ACTION_TYPE GetType() const = 0;

    virtual std::string SerializeData() const;

protected:
    virtual void _SerializeCustomData(Json::Value& value) const = 0;

    void _ReportPerformedStatus(bool performed);

private:
    virtual void _Redo() = 0;
    virtual void _Undo() = 0;

    void _DoSave(Database& db) override;
    void _DoSave(Database& db, Lock& dblock) override;
};

//! \brief Image was deleted (marked deleted)
class ImageDeleteAction final : public DatabaseAction {
    friend Database;

public:
    ImageDeleteAction(const std::vector<DBID>& images);
    ImageDeleteAction(DBID id, Database& from, const std::string& customdata);

    ~ImageDeleteAction();

    void Purge() override;

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

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    std::vector<DBID> ImagesToDelete;
};

} // namespace DV
