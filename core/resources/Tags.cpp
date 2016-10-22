// ------------------------------------ //
#include "Tags.h"

#include "core/Database.h"
#include "core/PreparedStatement.h"

#include "Common.h"
#include "Exceptions.h"

#include "Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //
// TagModifier
TagModifier::TagModifier(Database &db, Lock &dblock, PreparedStatement &statement,
    int64_t id) :
    DatabaseResource(id, db)
{
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "is_private");
    CheckRowID(statement, 3, "description");
    
    Name = statement.GetColumnAsString(1);
    Description = statement.GetColumnAsString(3);
    IsPrivate = statement.GetColumnAsBool(2);
}

void TagModifier::UpdateProperties(std::string name, std::string category, bool isprivate){
    
    if(!IsInDatabase())
        throw Leviathan::InvalidState("TagModifier not loaded from database");

    Name = name;
    IsPrivate = isprivate;
    Description = category;

    OnMarkDirty();

    Save();
}

// ------------------------------------ //
// TagData
std::string TagData::CreateInsertStatement(bool comment /*= false*/, bool allowfail
    /*= true*/)
{
    std::stringstream str;

    if(comment){
             
        str << "-- Tag '" << Name << "' ";

        if (Aliases.size() > 0)
            str << "with " << Aliases.size() << " alias(es)";

        str << "\n";
    }

    if(allowfail){
        str << "INSERT OR IGNORE INTO ";
    } else {
        str << "INSERT INTO ";
    }

    str << "tags (name, description, category, is_private) "
        "VALUES (\"" << Database::EscapeSql(Name) << "\", \"" <<
        Database::EscapeSql(Description) << "\", " << (int)Category << ", " <<
    (IsPrivate ? 1 : 0) << ");";
    
    for (const auto& alias : Aliases){
        
        str << "\n" << "INSERT " << (allowfail ? "OR IGNORE " : "")
            << "INTO tag_aliases (name, meant_tag) " 
            << "VALUES (\"" <<
            Database::EscapeSql(Leviathan::StringOperations::Replace<std::string>(
                    alias, "_", " ")) <<
            "\", (SELECT id FROM tags WHERE name = \"" << Database::EscapeSql(Name)
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

Tag::Tag(Database &db, Lock &dblock, PreparedStatement &statement,
    int64_t id) :
    DatabaseResource(id, db)
{
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "category");
    CheckRowID(statement, 3, "description");
    CheckRowID(statement, 4, "is_private");
    
    Name = statement.GetColumnAsString(1);
    Description = statement.GetColumnAsString(3);
    Category = static_cast<TAG_CATEGORY>(statement.GetColumnAsInt64(2));
    IsPrivate = statement.GetColumnAsBool(4);
}

void Tag::AddAlias(const std::string alias){

    if (alias.empty())
        return;
        
    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");
        
    InDatabase->InsertTagAlias(*this, alias);
}

void Tag::RemoveAlias(const std::string alias){

    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    InDatabase->DeleteTagAlias(*this, alias);
}

std::vector<std::shared_ptr<Tag>> Tag::GetImpliedTags() const{

    if(!IsInDatabase())
        throw Leviathan::InvalidState("Tag not loaded from database");

    return InDatabase->SelectTagImpliesAsTag();
}

// ------------------------------------ //
// AppliedTag
AppliedTag::AppliedTag(std::shared_ptr<Tag> tagonly) :
    DatabaseResource(true),
    MainTag(tagonly)
{
        
}

AppliedTag::AppliedTag(std::tuple<std::vector<std::shared_ptr<TagModifier>>,
    std::shared_ptr<Tag>> modifiersandtag) :
    AppliedTag(std::get<1>(modifiersandtag)) 
{
    Modifiers = std::get<0>(modifiersandtag);
}

AppliedTag::AppliedTag(std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<Tag>>
    composite) :
    AppliedTag(std::get<0>(composite))
{
    CombinedWith = std::make_tuple(std::get<1>(composite),
        std::make_shared<AppliedTag>(std::get<2>(composite)));
}

AppliedTag::AppliedTag(std::tuple<std::shared_ptr<Tag>, std::string,
    std::shared_ptr<AppliedTag>> composite) :
    AppliedTag(std::get<0>(composite))
{
    CombinedWith = std::make_tuple(std::get<1>(composite),
        std::get<2>(composite));
}
    
AppliedTag::AppliedTag(Database &db, Lock &dblock, PreparedStatement &statement,
    int64_t id) :
    DatabaseResource(id, db)
{
    CheckRowID(statement, 1, "tag");
    
    DBID tag;
    if(!statement.GetObjectIDFromColumn(tag, 1)){

        throw InvalidSQL("AppliedTag has no tag", 0, "");
    }
    
    Modifiers = db.SelectAppliedTagModifiers(dblock, ID);
    CombinedWith = db.SelectAppliedTagCombines(dblock, ID);
}

std::string AppliedTag::ToAccurateString() const {
    
    std::string result = "";

    for(auto& modifier : Modifiers){
        
        result += modifier->ToAccurateString() + " ";
    }

    result += MainTag->GetName();

    if(std::get<1>(CombinedWith)){
            
        result += " " + std::get<0>(CombinedWith) + " " +
            std::get<1>(CombinedWith)->ToAccurateString();
    }

    return result;
}

bool AppliedTag::IsSame(const AppliedTag &other) const {

    if(static_cast<const DatabaseResource&>(*this) !=
        static_cast<const DatabaseResource&>(other))
    {
        return false;
    }

    // Check for different modifiers //
    // Possibly different modifiers //
    if(Modifiers.size() != other.Modifiers.size())
        return false;
    
    // Must have the same modifiers (but they can be in different order) //
    for(const auto &modifier : Modifiers){

        bool found = false;

        for(const auto &otherModifier : other.Modifiers){

            if(*modifier == *otherModifier){

                found = true;
                break;
            }
        }

        if(!found)
            return false;
    }
    
    // They are the same //
    return true;
}



// ------------------------------------ //
// TagBreakRule

TagBreakRule::TagBreakRule(Database &db, Lock &dblock, PreparedStatement &statement,
    int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "tag_string");
    CheckRowID(statement, 2, "actual_tag");

    Pattern = statement.GetColumnAsString(1);
    const auto tagid = statement.GetColumnAsInt64(2);

    if(tagid){

        // Load the tag //
        ActualTag = db.SelectTagByID(dblock, tagid);
    }

    Modifiers = db.SelectModifiersForBreakRule(*this);
}

std::vector<std::shared_ptr<TagModifier>> TagBreakRule::DoBreak(std::string str,
    std::string &tagname, std::shared_ptr<Tag> &returnedtag)
{
    if (!Pattern.Contains('*'))
    {
        // Must be a direct match //
        if(CultureInfo.InvariantCulture.CompareInfo.Compare(str, Pattern, CompareOptions.IgnoreCase) != 0)
        {
            // Not a match //
            tagname = str;
            returnedtag = null;
            return null;
        }

        // Was a match //
        if(ActualTag == null)
        {
            throw new InvalidOperationException("full matching composite break rule must have a tag!");
        }

        tagname = ActualTag.Name;
        returnedtag = ActualTag;
        return Modifiers;
    }

    // A wildcard match //
    string[] wildcardparts = Pattern.Split('*');

    if(wildcardparts.Length != 2)
        throw new InvalidOperationException("composite break rule wildcard must have a single *");

    if (CultureInfo.InvariantCulture.CompareInfo.IndexOf(str, wildcardparts[0], CompareOptions.IgnoreCase) != 0)
    {
        // Not a match //
        // Despite best efforts this string doesn't match the rule //
        tagname = str;
        returnedtag = null;
        return null;
    }

    tagname = str.Substring(wildcardparts[0].Length).Trim();

    if(ActualTag != null)
    {
        if(CultureInfo.InvariantCulture.CompareInfo.Compare(
                tagname, ActualTag.Name, CompareOptions.IgnoreCase) != 0)
        {
            throw new InvalidOperationException("composite break rule wildcard matched a name that isn't " +
                "its own tag");
        }

        returnedtag = ActualTag;
        return Modifiers;
    }

    // Matched any tag pattern //
    returnedtag = null;
    return Modifiers;
}


void UpdateProperties(std::string newpattern, std::string newmaintag,
    std::vector<std::string> newmodifiers)
{

    if(!IsInDatabase())
        throw Leviathan::InvalidState("TagBreakRule not loaded from database");

    if (newpattern.Length < 1)
        throw Leviathan::InvalidArgument("Pattern cannot be empty");

    if (newmodifiers.Length < 1)
        throw Leviathan::InvalidArgument("BreakRule cannot be without modifiers, "
            "use an alias for that");

    Tag newtag = null;

    if(newmaintag.Length > 0)
    {
        newtag = FromDatabase.RetrieveTagByName(newmaintag);

        if(newtag == null)
        {
            throw Leviathan::InvalidArgument("New main tag doesn't exist");
        }
    }

    List<TagModifier> newmods = new List<TagModifier>();

    foreach(string mod in newmodifiers)
    {
        TagModifier applymod = FromDatabase.RetrieveTagModifierByName(mod);

        if (applymod == null)
        {
            throw Leviathan::InvalidArgument("New modifier '" + mod + "' doesn't exist");
        }

        newmods.Add(applymod);
    }

    ActualTag = newtag;
    Modifiers = newmods;
    Pattern = newpattern;

    OnMarkDirty();
    Save();
}




