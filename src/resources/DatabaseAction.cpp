// ------------------------------------ //
#include "DatabaseAction.h"

#include "resources/ResourceWithPreview.h"

#include "Database.h"
#include "DualView.h"
#include "SQLHelpers.h"
#include "resources/Image.h"

#include "Exceptions.h"
#include "json/json.h"

using namespace DV;
// ------------------------------------ //
DatabaseAction::DatabaseAction() : DatabaseResource(true) {}

DatabaseAction::DatabaseAction(DBID id, Database& from) : DatabaseResource(id, from) {}

DatabaseAction::~DatabaseAction() {}
// ------------------------------------ //
std::shared_ptr<DatabaseAction> DatabaseAction::Create(
    Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id)
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
    case DATABASE_ACTION_TYPE::ImageMerge:
        return std::shared_ptr<ImageMergeAction>(new ImageMergeAction(
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
        Performed = false;
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
        Performed = true;
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

    GUARD_LOCK();
    NotifyAll(guard);
}
// ------------------------------------ //
void DatabaseAction::_DoSave(Database& db)
{
    db.UpdateDatabaseActionAG(*this);
}
void DatabaseAction::_DoSave(Database& db, DatabaseLockT& dblock)
{
    db.UpdateDatabaseAction(dblock, *this);
}
// ------------------------------------ //
void DatabaseAction::_OnPurged()
{
    Deleted = true;

    GUARD_LOCK();
    NotifyAll(guard);
}

std::vector<std::shared_ptr<ResourceWithPreview>> DatabaseAction::LoadPreviewItems(
    int max /*= 10*/) const
{
    return {};
}

void DatabaseAction::OpenEditingWindow()
{
    throw Leviathan::InvalidType("This action does not support opening editing window");
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
// ------------------------------------ //
std::string ImageDeleteAction::GenerateDescription() const
{
    std::stringstream description;

    description << "Deleted ";

    if(ImagesToDelete.size() > 1) {
        description << ImagesToDelete.size() << " images";
    } else {
        description << "an image";
    }

    return description.str();
}

std::vector<std::shared_ptr<ResourceWithPreview>> ImageDeleteAction::LoadPreviewItems(
    int max) const
{
    if(!InDatabase || ImagesToDelete.empty())
        return {};

    std::vector<std::shared_ptr<ResourceWithPreview>> result;
    result.reserve(std::min<size_t>(max, ImagesToDelete.size()));

    GUARD_LOCK_OTHER(InDatabase);

    for(size_t i = 0; i < ImagesToDelete.size() && i < static_cast<size_t>(max); ++i) {

        auto casted = std::dynamic_pointer_cast<ResourceWithPreview>(
            InDatabase->SelectImageByID(guard, ImagesToDelete[i]));

        if(casted)
            result.push_back(casted);
    }

    return result;
}
// ------------------------------------ //
// ImageMergeAction
ImageMergeAction::ImageMergeAction(DBID mergetarget, const std::vector<DBID>& images) :
    Target(mergetarget), ImagesToMerge(images)
{}

ImageMergeAction::ImageMergeAction(
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

    {
        const auto& images = value["images"];

        ImagesToMerge.reserve(images.size());

        for(const auto& image : images) {

            ImagesToMerge.push_back(image.asInt64());
        }
    }

    Target = value["target"].asInt64();

    {
        const auto& tags = value["tags"];

        AddTagsToTarget.reserve(tags.size());

        for(const auto& tag : tags) {

            AddTagsToTarget.push_back(tag.asString());
        }
    }

    {
        const auto& collections = value["collections"];

        AddTargetToCollections.reserve(collections.size());

        for(const auto& data : collections) {

            const auto collection = data["collection"].asInt64();
            const auto order = data["order"].asInt64();

            AddTargetToCollections.push_back(std::make_tuple(collection, order));
        }
    }
}

ImageMergeAction::~ImageMergeAction()
{
    DBResourceDestruct();
}
// ------------------------------------ //
bool ImageMergeAction::IsSame(
    const Image& target, const std::vector<std::shared_ptr<Image>>& images) const
{
    if(images.size() != ImagesToMerge.size())
        return false;

    return Target == target.GetID() &&
           std::equal(ImagesToMerge.begin(), ImagesToMerge.end(), images.begin(),
               [](DBID first, const std::shared_ptr<Image>& second) {
                   return first == second->GetID();
               });
}
// ------------------------------------ //
void ImageMergeAction::UpdateProperties(DBID target, const std::vector<DBID>& imagestomerge)
{
    Target = target;
    ImagesToMerge = imagestomerge;
    OnMarkDirty();
}
// ------------------------------------ //
void ImageMergeAction::_Redo()
{
    InDatabase->RedoAction(*this);
}

void ImageMergeAction::_Undo()
{
    InDatabase->UndoAction(*this);
}

void ImageMergeAction::_OnPurged()
{
    DatabaseAction::_OnPurged();
    InDatabase->PurgeAction(*this);
}

void ImageMergeAction::_SerializeCustomData(Json::Value& value) const
{
    Json::Value images;
    images.resize(ImagesToMerge.size());
    for(size_t i = 0; i < images.size(); ++i) {
        images[static_cast<unsigned>(i)] = ImagesToMerge[i];
    }

    Json::Value tags;
    tags.resize(AddTagsToTarget.size());
    for(size_t i = 0; i < tags.size(); ++i) {
        tags[static_cast<unsigned>(i)] = AddTagsToTarget[i];
    }

    Json::Value collections;
    collections.resize(AddTargetToCollections.size());
    for(size_t i = 0; i < collections.size(); ++i) {
        Json::Value element;
        element["collection"] = std::get<0>(AddTargetToCollections[i]);
        element["order"] = std::get<1>(AddTargetToCollections[i]);
        collections[static_cast<unsigned>(i)] = element;
    }

    value["images"] = images;
    value["target"] = Target;
    value["tags"] = tags;
    value["collections"] = collections;
}
// ------------------------------------ //
std::string ImageMergeAction::GenerateDescription() const
{
    std::stringstream description;

    description << "Merged ";

    if(ImagesToMerge.size() != 1) {
        description << ImagesToMerge.size() << " images ";
    } else {
        description << "an image ";
    }

    description << "into "
                << (InDatabase ? InDatabase->SelectImageNameByIDAG(Target) :
                                 std::to_string(Target));

    return description.str();
}

std::vector<std::shared_ptr<ResourceWithPreview>> ImageMergeAction::LoadPreviewItems(
    int max) const
{
    if(!InDatabase || max <= 0)
        return {};

    std::vector<std::shared_ptr<ResourceWithPreview>> result;
    result.reserve(std::min<size_t>(max, ImagesToMerge.size()));

    max -= 1;

    GUARD_LOCK_OTHER(InDatabase);

    auto casted = std::dynamic_pointer_cast<ResourceWithPreview>(
        InDatabase->SelectImageByID(guard, Target));

    if(casted)
        result.push_back(casted);

    for(size_t i = 0; i < ImagesToMerge.size() && i < static_cast<size_t>(max); ++i) {

        auto casted = std::dynamic_pointer_cast<ResourceWithPreview>(
            InDatabase->SelectImageByID(guard, ImagesToMerge[i]));

        if(casted)
            result.push_back(casted);
    }

    return result;
}
// ------------------------------------ //
void ImageMergeAction::OpenEditingWindow()
{
    DualView::Get().OpenActionEdit(
        std::dynamic_pointer_cast<std::remove_pointer_t<decltype(this)>>(shared_from_this()));
}
