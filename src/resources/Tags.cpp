// ------------------------------------ //
#include "Tags.h"

#include "Database.h"
#include "DualView.h"
#include "PreparedStatement.h"

#include "Common.h"
#include "Exceptions.h"

#include "Common/StringOperations.h"

#include <boost/algorithm/string.hpp>

using namespace DV;
// ------------------------------------ //
// TagModifier
TagModifier::TagModifier(
    Database& db, Lock& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db)
{
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "is_private");
    CheckRowID(statement, 3, "description");
    CheckRowID(statement, 4, "deleted");

    Name = statement.GetColumnAsString(1);
    Description = statement.GetColumnAsString(3);
    IsPrivate = statement.GetColumnAsBool(2);
    Deleted = statement.GetColumnAsOptionalBool(4);
}

TagModifier::~TagModifier()
{
    DBResourceDestruct();
}

void TagModifier::UpdateProperties(std::string name, std::string category, bool isprivate)
{
    if(!IsInDatabase())
        throw Leviathan::InvalidState("TagModifier not loaded from database");

    Name = name;
    IsPrivate = isprivate;
    Description = category;

    OnMarkDirty();

    Save();
}

void TagModifier::_DoSave(Database& db)
{
    db.UpdateTagModifier(*this);
}

// ------------------------------------ //
// TagData
std::string TagData::CreateInsertStatement(bool comment /*= false*/, bool allowfail
    /*= true*/)
{
    std::stringstream str;

    if(comment) {

        str << "-- Tag '" << Name << "' ";

        if(Aliases.size() > 0)
            str << "with " << Aliases.size() << " alias(es)";

        str << "\n";
    }

    if(allowfail) {
        str << "INSERT OR IGNORE INTO ";
    } else {
        str << "INSERT INTO ";
    }

    str << "tags (name, description, category, is_private) "
           "VALUES (\""
        << Database::EscapeSql(Name) << "\", \"" << Database::EscapeSql(Description) << "\", "
        << (int)Category << ", " << (IsPrivate ? 1 : 0) << ");";

    for(const auto& alias : Aliases) {

        str << "\n"
            << "INSERT " << (allowfail ? "OR IGNORE " : "")
            << "INTO tag_aliases (name, meant_tag) "
            << "VALUES (\""
            << Database::EscapeSql(
                   Leviathan::StringOperations::Replace<std::string>(alias, "_", " "))
            << "\", (SELECT id FROM tags WHERE name = \"" << Database::EscapeSql(Name)
            << "\"));";
    }

    return str.str();
}

// ------------------------------------ //
// Tag

Tag::Tag(std::string name, std::string description, TAG_CATEGORY category,
    bool isprivate /*= false*/) :
    DatabaseResource(true)
{
    Name = name;
    Description = description;
    IsPrivate = isprivate;
    Category = category;
}

Tag::Tag(Database& db, Lock& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db)
{
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "category");
    CheckRowID(statement, 3, "description");
    CheckRowID(statement, 4, "is_private");
    CheckRowID(statement, 5, "example_image_region");
    CheckRowID(statement, 6, "deleted");


    Name = statement.GetColumnAsString(1);
    Description = statement.GetColumnAsString(3);
    Category = static_cast<TAG_CATEGORY>(statement.GetColumnAsInt64(2));
    IsPrivate = statement.GetColumnAsBool(4);
    Deleted = statement.GetColumnAsOptionalBool(6);
}

Tag::~Tag()
{
    DBResourceDestruct();
}
// ------------------------------------ //
void Tag::AddAlias(const std::string alias)
{
    if(alias.empty())
        return;

    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    InDatabase->InsertTagAlias(*this, alias);
}

void Tag::RemoveAlias(const std::string alias)
{
    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    InDatabase->DeleteTagAlias(*this, alias);
}

std::vector<std::string> Tag::GetAliases() const
{
    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    return InDatabase->SelectTagAliases(*this);
}
// ------------------------------------ //
void Tag::AddImpliedTag(std::shared_ptr<Tag> imply)
{
    if(!imply)
        return;

    if(!IsInDatabase())
        return;

    InDatabase->InsertTagImply(*this, *imply);
}

void Tag::RemoveImpliedTag(std::shared_ptr<Tag> imply)
{
    if(!imply)
        return;

    if(!IsInDatabase())
        return;

    InDatabase->DeleteTagImply(*this, *imply);
}

std::vector<std::shared_ptr<Tag>> Tag::GetImpliedTags() const
{
    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    return InDatabase->SelectTagImpliesAsTag(*this);
}

bool Tag::operator==(const Tag& other) const
{
    if(static_cast<const DatabaseResource&>(*this) ==
        static_cast<const DatabaseResource&>(other)) {
        return true;
    }

    return Name == other.Name;
}

void Tag::_DoSave(Database& db)
{
    db.UpdateTag(*this);
}
// ------------------------------------ //
// AppliedTag
AppliedTag::AppliedTag(std::shared_ptr<Tag> tagonly) : MainTag(tagonly) {}

AppliedTag::AppliedTag(
    std::shared_ptr<Tag> tag, std::vector<std::shared_ptr<TagModifier>> modifiers) :
    MainTag(tag),
    Modifiers(modifiers)
{}

AppliedTag::AppliedTag(
    std::tuple<std::vector<std::shared_ptr<TagModifier>>, std::shared_ptr<Tag>>
        modifiersandtag) :
    AppliedTag(std::get<1>(modifiersandtag))
{
    Modifiers = std::get<0>(modifiersandtag);
}

AppliedTag::AppliedTag(
    std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<Tag>> composite) :
    AppliedTag(std::get<0>(composite))
{
    CombinedWith = std::make_tuple(
        std::get<1>(composite), std::make_shared<AppliedTag>(std::get<2>(composite)));
}

AppliedTag::AppliedTag(
    std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<AppliedTag>> composite) :
    AppliedTag(std::get<0>(composite))
{
    CombinedWith = std::make_tuple(std::get<1>(composite), std::get<2>(composite));
}

AppliedTag::AppliedTag(Database& db, Lock& dblock, PreparedStatement& statement, int64_t id) :
    ID(id)
{
    CheckRowID(statement, 1, "tag");

    DBID tag;
    if(!statement.GetObjectIDFromColumn(tag, 1)) {

        throw InvalidSQL("AppliedTag has no tag", 0, "");
    }

    MainTag = db.SelectTagByID(dblock, tag);

    if(!MainTag)
        throw Leviathan::InvalidState("AppliedTag loaded from database failed to "
                                      "find maintag: " +
                                      Convert::ToString(tag));

    Modifiers = db.SelectAppliedTagModifiers(dblock, *this);
    CombinedWith = db.SelectAppliedTagCombine(dblock, *this);
}

std::string AppliedTag::ToAccurateString() const
{
    std::string result = "";

    for(auto& modifier : Modifiers) {

        result += modifier->ToAccurateString() + " ";
    }

    result += MainTag->GetName();

    if(std::get<1>(CombinedWith)) {

        result += " " + std::get<0>(CombinedWith) + " " +
                  std::get<1>(CombinedWith)->ToAccurateString();
    }

    return result;
}

std::string AppliedTag::GetTagName() const
{
    if(!MainTag)
        throw Leviathan::InvalidState("AppliedTag has no Tag to get name from");

    return MainTag->GetName();
}

void AppliedTag::SetCombineWith(const std::string& middle, std::shared_ptr<AppliedTag> right)
{
    if(middle.empty())
        throw Leviathan::InvalidArgument("AppliedTag: setting combined with empty string");

    CombinedWith = std::make_tuple(middle, right);
}

bool AppliedTag::GetCombinedWith(
    std::string& combinestr, std::shared_ptr<AppliedTag>& combined) const
{
    if(!std::get<1>(CombinedWith))
        return false;

    combined = std::get<1>(CombinedWith);
    combinestr = std::get<0>(CombinedWith);

    if(combinestr.empty())
        throw Leviathan::InvalidState("AppliedTag: combined tag has empty combined with str");

    return true;
}


bool AppliedTag::IsSame(const AppliedTag& other) const
{
    // Check tag id
    if((MainTag.operator bool()) != (other.MainTag.operator bool()))
        return false;

    if(*MainTag != *other.MainTag)
        return false;

    // Check for different modifiers //
    // Possibly different modifiers //
    if(Modifiers.size() != other.Modifiers.size())
        return false;

    // Must have the same modifiers (but they can be in different order) //
    for(const auto& modifier : Modifiers) {

        bool found = false;

        for(const auto& otherModifier : other.Modifiers) {

            if(*modifier == *otherModifier) {

                found = true;
                break;
            }
        }

        if(!found)
            return false;
    }

    // Check combine //
    if((std::get<1>(CombinedWith).get() != nullptr) !=
        (std::get<1>(other.CombinedWith).get() != nullptr)) {
        // Combined with isn't the same
        return false;
    }

    // Check combined //
    if(std::get<1>(CombinedWith)) {

        // The other must also have a combined with as we checked above that both either
        // have or don't have a combined with
        if(std::get<0>(CombinedWith) != std::get<0>(other.CombinedWith)) {

            // Different combine word
            return false;
        }

        // The combined with tags need to be same //
        return std::get<1>(CombinedWith)->IsSame(*std::get<1>(other.CombinedWith));
    }

    // They are the same //
    return true;
}



// ------------------------------------ //
// TagBreakRule

TagBreakRule::TagBreakRule(
    Database& db, Lock& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "tag_string");
    CheckRowID(statement, 2, "actual_tag");

    Pattern = statement.GetColumnAsString(1);
    const auto tagid = statement.GetColumnAsInt64(2);

    if(tagid) {

        // Load the tag //
        ActualTag = db.SelectTagByID(dblock, tagid);
    }

    Modifiers = db.SelectModifiersForBreakRule(dblock, *this);
}

TagBreakRule::~TagBreakRule()
{
    DBResourceDestruct();
}

std::vector<std::shared_ptr<TagModifier>> TagBreakRule::DoBreak(
    std::string str, std::string& tagname, std::shared_ptr<Tag>& returnedtag)
{
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if(str.empty()) {

        // Doesn't match //
        tagname = str;
        returnedtag = nullptr;
        return {};
    }

    if(Pattern.find_first_of('*') == std::string::npos) {

        // Must be a direct match //
        if(!boost::iequals(Pattern, str)) {

            // Not a match //
            tagname = str;
            returnedtag = nullptr;
            return {};
        }

        // Was a match //
        if(!ActualTag) {

            throw Leviathan::InvalidState("full matching composite break rule must "
                                          "have a tag!");
        }

        tagname = ActualTag->GetName();
        returnedtag = ActualTag;
        return Modifiers;
    }

    // A wildcard match //
    std::vector<boost::iterator_range<std::string::iterator>> wildcardparts;

    boost::split(wildcardparts, Pattern, boost::is_any_of("*"));

    if(wildcardparts.size() != 2)
        throw Leviathan::InvalidState("composite break rule wildcard must have a single *");

    tagname = std::string(wildcardparts[0].begin(), wildcardparts[0].end());

    if(tagname.empty()) {

        // Try the second part //
        tagname = std::string(wildcardparts[1].begin(), wildcardparts[1].end());
    }

    if(!boost::iequals(tagname, str)) {

        // Not a match //
        // Despite best efforts this string doesn't match the rule //
        tagname = str;
        returnedtag = nullptr;
        return {};
    }

    // It matched, but just in case there was whitespace trim it //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(tagname);

    // A sanity check //
    if(ActualTag) {

        if(boost::iequals(tagname, ActualTag->GetName())) {

            throw Leviathan::InvalidState("composite break rule wildcard matched a name "
                                          "that isn't its own tag");
        }

        returnedtag = ActualTag;
        return Modifiers;
    }

    // Matched any tag pattern //
    returnedtag = nullptr;
    return Modifiers;
}

void TagBreakRule::_DoSave(Database& db)
{
    db.UpdateTagBreakRule(*this);
}

void TagBreakRule::UpdateProperties(
    std::string newpattern, std::string newmaintag, std::vector<std::string> newmodifiers)
{
    if(!IsInDatabase())
        throw Leviathan::InvalidState("TagBreakRule not loaded from database");

    if(newpattern.empty())
        throw Leviathan::InvalidArgument("Pattern cannot be empty");

    if(newmodifiers.empty())
        throw Leviathan::InvalidArgument("BreakRule cannot be without modifiers, "
                                         "use an alias for that");

    std::shared_ptr<Tag> newtag = nullptr;

    if(!newmaintag.empty()) {

        newtag = InDatabase->SelectTagByNameAG(newmaintag);

        if(!newtag) {

            throw Leviathan::InvalidArgument("New main tag doesn't exist");
        }
    }

    std::vector<std::shared_ptr<TagModifier>> newmods;
    newmods.reserve(newmodifiers.size());

    for(auto& mod : newmodifiers) {

        auto applymod = InDatabase->SelectTagModifierByNameAG(mod);

        if(!applymod) {

            throw Leviathan::InvalidArgument("New modifier '" + mod + "' doesn't exist");
        }

        newmods.push_back(applymod);
    }

    ActualTag = newtag;
    Modifiers = newmods;
    Pattern = newpattern;

    OnMarkDirty();
    Save();
}

// ------------------------------------ //
// TagCollection
TagCollection::TagCollection(std::vector<std::shared_ptr<AppliedTag>> tags) : Tags(tags)
{
    for(auto& tag : Tags) {
        if(!tag)
            throw Leviathan::InvalidArgument("Constructing TagCollection that "
                                             "has a null tag");
    }
}

TagCollection::TagCollection() {}
// ------------------------------------ //
bool TagCollection::HasTag(const AppliedTag& tagtocheck)
{
    CheckIsLoaded();

    for(const auto& tag : Tags) {

        if(tag->IsSame(tagtocheck))
            return true;
    }

    return false;
}

void TagCollection::Clear()
{
    CheckIsLoaded();

    for(const auto& tag : Tags) {

        _TagRemoved(*tag);
    }

    Tags.clear();
}
// ------------------------------------ //
bool TagCollection::RemoveTag(const AppliedTag& exacttag)
{
    CheckIsLoaded();

    for(auto iter = Tags.begin(); iter != Tags.end(); ++iter) {

        if((*iter)->IsSame(exacttag)) {

            _TagRemoved(**iter);
            Tags.erase(iter);
            return true;
        }
    }

    return false;
}

bool TagCollection::RemoveText(const std::string& str)
{
    CheckIsLoaded();

    for(auto iter = Tags.begin(); iter != Tags.end(); ++iter) {

        if((*iter)->ToAccurateString() == str) {

            _TagRemoved(**iter);
            Tags.erase(iter);
            return true;
        }
    }

    return false;
}

bool TagCollection::Add(std::shared_ptr<Tag> tag)
{
    if(!tag)
        return false;

    auto toadd = std::make_shared<AppliedTag>(tag);

    return Add(toadd);
}

bool TagCollection::Add(std::shared_ptr<AppliedTag> tag)
{
    if(!tag || HasTag(*tag))
        return false;

    if(!tag->GetTag())
        return false;

    Tags.push_back(tag);
    _TagAdded(*tag);
    return true;
}

bool TagCollection::Add(std::shared_ptr<AppliedTag> tag, Lock& dblock)
{
    if(!tag || HasTag(*tag))
        return false;

    if(!tag->GetTag())
        return false;

    Tags.push_back(tag);
    _TagAdded(*tag, dblock);
    return true;
}

void TagCollection::Add(const TagCollection& other)
{
    CheckIsLoaded();

    for(const auto& tag : other) {

        Add(tag);
    }
}

void TagCollection::Add(const TagCollection& other, Lock& dblock)
{
    CheckIsLoaded(dblock);

    for(const auto& tag : other) {

        Add(tag, dblock);
    }
}

size_t TagCollection::GetTagCount()
{
    CheckIsLoaded();

    return Tags.size();
}
// ------------------------------------ //
void TagCollection::ReplaceWithText(const std::string& text, const std::string& separator)
{
    CheckIsLoaded();
    Clear();

    AddTextTags(text, separator);
}

void TagCollection::AddTextTags(const std::string& text, const std::string& separator)
{
    CheckIsLoaded();

    std::vector<std::string> tagparts;
    Leviathan::StringOperations::CutString<std::string>(text, separator, tagparts);

    for(auto& line : tagparts) {

        if(line.empty())
            continue;

        try {
            auto tag = DualView::Get().ParseTagFromString(line);

            if(!tag)
                continue;

            Add(tag);

        } catch(const Leviathan::InvalidArgument& e) {
            // TODO: should do a popup to let user know
            LOG_WARNING("AddTextTags: failed to parse tag: " + line + ", exception:");
            e.PrintToLog();
            continue;
        }
    }
}

void TagCollection::ReplaceWithText(std::string text)
{
    CheckIsLoaded();

    std::vector<std::string> lines;
    Leviathan::StringOperations::CutLines<std::string>(text, lines);

    Clear();

    for(auto& line : lines) {

        if(line.empty())
            continue;

        auto tag = DualView::Get().ParseTagFromString(line);

        if(!tag)
            continue;

        Add(tag);
    }
}
// ------------------------------------ //
std::string TagCollection::TagsAsString(const std::string& separator)
{
    CheckIsLoaded();

    std::vector<std::string> strs;
    strs.reserve(Tags.size());

    for(const auto& tag : Tags) {

        if(tag->HasDeletedParts())
            continue;

        strs.push_back(tag->ToAccurateString());
    }

    return Leviathan::StringOperations::StitchTogether(strs, separator);
}


// ------------------------------------ //
// DatabaseTagCollection
void DatabaseTagCollection::RefreshTags()
{
    TagsLoaded = true;

    Tags.clear();

    GUARD_LOCK_OTHER(LoadedDB);

    LoadTags(guard, Tags);
}

void DatabaseTagCollection::_OnCheckTagsLoaded()
{
    if(TagsLoaded)
        return;

    TagsLoaded = true;

    // Load tags from the database //
    GUARD_LOCK_OTHER(LoadedDB);
    LoadTags(guard, Tags);
}

void DatabaseTagCollection::_OnCheckTagsLoaded(Lock& dblock)
{
    if(TagsLoaded)
        return;

    TagsLoaded = true;

    // Load tags from the database //
    LoadTags(dblock, Tags);
}
// ------------------------------------ //
void DatabaseTagCollection::_TagRemoved(AppliedTag& tag)
{
    GUARD_LOCK_OTHER(LoadedDB);
    OnRemoveTag(guard, tag);
}

void DatabaseTagCollection::_TagAdded(AppliedTag& tag)
{
    GUARD_LOCK_OTHER(LoadedDB);
    OnAddTag(guard, tag);
}

void DatabaseTagCollection::_TagAdded(AppliedTag& tag, Lock& dblock)
{
    OnAddTag(dblock, tag);
}
