// ------------------------------------ //
#include "DatabaseAction.h"

#include "Database.h"
#include "SQLHelpers.h"

#include "Exceptions.h"
#include "json/json.h"

using namespace DV;
// ------------------------------------ //
DatabaseAction::DatabaseAction() : DatabaseResource(true) {}

DatabaseAction::DatabaseAction(DBID id, Database& from) : DatabaseResource(id, from) {}

DatabaseAction::~DatabaseAction() {}
// ------------------------------------ //
std::shared_ptr<DatabaseAction> DatabaseAction::Create(
    Database& db, Lock& dblock, PreparedStatement& statement, DBID id)
{
    CheckRowID(statement, 1, "type");
    CheckRowID(statement, 2, "performed");
    CheckRowID(statement, 3, "json_data");

    const auto uncastedType = statement.GetColumnAsInt(1);

    if(uncastedType <= 0 || uncastedType >= static_cast<int>(DATABASE_ACTION_TYPE::Invalid)) {
        LOG_ERROR(
            "DatabaseAction: from DB read type is invalid:" + std::to_string(uncastedType));
        return nullptr;
    }

    const auto type = static_cast<DATABASE_ACTION_TYPE>(uncastedType);

    switch(type) {
    case DATABASE_ACTION_TYPE::ImageDelete:
        return std::shared_ptr<ImageDeleteAction>(new ImageDeleteAction(
            id, db, statement.GetColumnAsBool(2), statement.GetColumnAsString(3)));
    case DATABASE_ACTION_TYPE::Invalid:
        LOG_ERROR("DatabaseAction: from DB read type is DATABASE_ACTION_TYPE::Invalid");
        return nullptr;
    }

    // GCC complains about being able to reach here
    LEVIATHAN_ASSERT(false, "This should not be reachable");
    return nullptr;
}
// ------------------------------------ //
bool DatabaseAction::Redo()
{
    if(IsPerformed())
        return false;

    try {
        _Redo();
    } catch(const InvalidSQL& e) {

        LOG_INFO("Error happened in DatabaseAction::Redo:");
        e.PrintToLog();
        return false;
    }

    if(!Performed) {
        LOG_ERROR(
            "DatabaseAction: performed status didn't change after completing internal redo");
    }
    return true;
}

bool DatabaseAction::Undo()
{
    if(!IsPerformed())
        return false;

    try {
        _Undo();
    } catch(const InvalidSQL& e) {

        LOG_INFO("Error happened in DatabaseAction::Undo:");
        e.PrintToLog();
        return false;
    }

    if(Performed) {
        LOG_ERROR(
            "DatabaseAction: performed status didn't change after completing internal undo");
    }

    return true;
}
// ------------------------------------ //
std::string DatabaseAction::SerializeData() const
{
    std::stringstream sstream;
    Json::Value value;

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    auto writer = builder.newStreamWriter();

    _SerializeCustomData(value);

    writer->write(value, &sstream);

    return sstream.str();
}
// ------------------------------------ //
void DatabaseAction::_ReportPerformedStatus(bool performed)
{
    Performed = performed;
}
// ------------------------------------ //
void DatabaseAction::_DoSave(Database& db)
{
    // TODO: check if this needs to work
    DEBUG_BREAK;
}
void DatabaseAction::_DoSave(Database& db, Lock& dblock)
{
    DEBUG_BREAK;
}
// ------------------------------------ //
void DatabaseAction::_OnPurged()
{
    Deleted = true;
}

// ------------------------------------ //
// ImageDeleteAction
ImageDeleteAction::ImageDeleteAction(const std::vector<DBID>& images) : ImagesToDelete(images)
{}

ImageDeleteAction::ImageDeleteAction(
    DBID id, Database& from, bool performed, const std::string& customdata) :
    DatabaseAction(id, from)
{
    Performed = performed;

    std::stringstream sstream(customdata);

    Json::CharReaderBuilder builder;
    Json::Value value;
    JSONCPP_STRING errs;

    if(!parseFromStream(builder, sstream, &value, &errs))
        throw InvalidArgument("invalid json:" + errs);

    const auto& images = value["images"];

    ImagesToDelete.reserve(images.size());

    for(const auto& image : images) {

        ImagesToDelete.push_back(image.asInt64());
    }
}

ImageDeleteAction::~ImageDeleteAction()
{
    DBResourceDestruct();
}
// ------------------------------------ //
void ImageDeleteAction::_Redo()
{
    InDatabase->RedoAction(*this);
}

void ImageDeleteAction::_Undo()
{
    InDatabase->UndoAction(*this);
}

void ImageDeleteAction::_OnPurged()
{
    DatabaseAction::_OnPurged();
    InDatabase->PurgeAction(*this);
}

void ImageDeleteAction::_SerializeCustomData(Json::Value& value) const
{
    Json::Value images;
    images.resize(ImagesToDelete.size());
    for(size_t i = 0; i < images.size(); ++i) {
        images[static_cast<unsigned>(i)] = ImagesToDelete[i];
    }

    value["images"] = images;
}
