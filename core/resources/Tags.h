#pragma once

#include "DatabaseResource.h"

#include "Common.h"

namespace DV{

class PreparedStatement;

//! Represents a word that is placed before tag, like "red flower"
class TagModifier : public DatabaseResource {
public:
    
    TagModifier(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);
    
    std::string ToAccurateString(){
        
        return Name;
    }

    bool operator ==(const TagModifier &other) const{

        return Name == other.Name;
    }

    const auto GetName() const{

        return Name;
    }

    const auto GetDescription() const{
        
        return Description;
    }

    const auto GetIsPrivate() const{
        
        return IsPrivate;
    }

    void UpdateProperties(std::string name, std::string category, bool isprivate);

protected:

    void _DoSave(Database &db) override;
    
protected:
    
    std::string Name;
    bool IsPrivate;
    std::string Description;
};

//! Holds data that Tag has, used to create non-database tag objects
class TagData{
public:
    
    //! Creates an sql statement that can be used to insert this into the database
    std::string CreateInsertStatement(bool comment = false, bool allowfail = true);

public:
    
    //! The text of the tag
    std::string Name;
    
    //! Description string of the tag
    std::string Description = "";
    
    bool IsPrivate = false;
    
    TAG_CATEGORY Category = TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT;

    //! Aliases of this tag
    std::vector<std::string> Aliases;

    //! List of IDs that are implied by this tag
    std::vector<DBID> Implies;
};

//! Represents a tag that can be applied to images or collections
class Tag : public DatabaseResource{
public:
    //! Can be used to mark tags as selected, used at least for exporting downloaded tags
    bool Selected = false;

    //! Creates a tag for adding to the database
    Tag(std::string name, std::string description, TAG_CATEGORY category,
        bool isprivate = false);

    Tag(Database &db, Lock &dblock, PreparedStatement &statement,
        int64_t id);

    bool operator <(const Tag &other) const{

        return Name < other.Name;
    }

    bool operator !=(const Tag &other) const{

        return !(*this == other);
    }

    bool operator ==(const Tag &other) const;

    void SetName(const std::string &name){

        Name = name;
        OnMarkDirty();
    }

    const auto GetName() const{

        return Name;
    }


    const auto GetCategory() const{
        
        return Category;
    }
 
    const auto GetDescription() const{
        
        return Description;
    }

    const auto GetIsPrivate() const{
        
        return IsPrivate;
    }

    void AddAlias(const std::string alias);
    
    void RemoveAlias(const std::string alias);

    std::vector<std::shared_ptr<Tag>> GetImpliedTags() const;


protected:

    void _DoSave(Database &db) override;
    
protected:
    

    //! The text of the tag
    std::string Name;
    
    //! Description string of the tag
    std::string Description;
    
    bool IsPrivate;
    TAG_CATEGORY Category = TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT;
};


//! Represents an imply relationship between two tags
//! When the ImpliedBy tag is applied also the Primary tag should be assumed to be applied
//! \todo Allow loading and saving these from the database
class ImpliedTag /*: public DatabaseResource*/{
public:

    ImpliedTag(std::shared_ptr<Tag> tag, std::shared_ptr<Tag> impliedby) :
        /*DatabaseResource(false),*/
        Primary(tag),
        ImpliedBy(impliedby)
    {
    }

    std::string GetImplySqlComment(){
        return "-- Implied tag '" + ImpliedBy->GetName() + "' => '" + Primary->GetName() +
        "' \n";
    }

    
    //! Creates an sql statement that can be used to insert this into the database
    std::string CreateInsertStatement(bool comment = false, bool allowfail = true)
    {
        std::string str = comment ? GetImplySqlComment() : "";

        str += "INSERT ";
        str += (allowfail ? "OR IGNORE " : "");
        str += "INTO tag_implies (primary_tag, to_apply) VALUES (";
        str += "(SELECT id FROM tags WHERE name = \"" + ImpliedBy->GetName() +
            "\"), (SELECT id FROM tags WHERE name = \"" + Primary->GetName() + "\"));";
        
        if (comment)
            str += "\n";

        return str;
    }

protected:
        
    std::shared_ptr<Tag> Primary;
    std::shared_ptr<Tag> ImpliedBy;
};

//! \brief Used to split a string into tags according to a rule
class TagBreakRule : public DatabaseResource {
public:
    
    TagBreakRule(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);
    
    //! Breaks a string according to this rule
    //! \returns The modifiers that were before the tag
    //! \param tagname If break succeeds this contains the name of the tag
    //! \param returnedtag If break succeeds this contains the tag
    std::vector<std::shared_ptr<TagModifier>> DoBreak(std::string str, std::string &tagname,
        std::shared_ptr<Tag> &returnedtag);

    void UpdateProperties(std::string newpattern, std::string newmaintag,
        std::vector<std::string> newmodifiers);
    
protected:

    std::string Pattern;
    std::shared_ptr<Tag> ActualTag;
    std::vector<std::shared_ptr<TagModifier>> Modifiers;
};




//! A full tag that is applied to something
class AppliedTag{
public:

    //! Creates an applied tag for a tag
    AppliedTag(std::shared_ptr<Tag> tagonly);

    AppliedTag(std::tuple<std::vector<std::shared_ptr<TagModifier>>,
        std::shared_ptr<Tag>> modifiersandtag);

    //! \brief Creates a combined tag with a string in between
    //! \note Implicitly creates a new applied tag from the second tag
    AppliedTag(std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<Tag>> composite);

    //! Creates a composite with an existing right hand side of the composite already created
    AppliedTag(std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<AppliedTag>>
        composite);

    AppliedTag(Database &db, Lock &dblock, PreparedStatement &statement,
        int64_t id);

    //! Alias for IsSame
    bool operator ==(const AppliedTag &other) const{

        return IsSame(other);
    }
    
    bool IsSame(const AppliedTag &other) const;

    //! Creates a string representation that can be parsed with DualView::ParseTagFromString
    std::string ToAccurateString() const;

    //! \brief Returns true if CombinedWith is set. And returns the values in the parameters
    bool GetCombinedWith(std::string &combinestr, std::shared_ptr<AppliedTag> &combined) const{

        if(!std::get<1>(CombinedWith))
            return false;
        
        combined = std::get<1>(CombinedWith);
        combinestr = std::get<0>(CombinedWith);
        return true;
    }

    //! \brief Sets the combine with
    //! \todo Assert if called on one loaded from database
    void SetCombineWith(const std::string &middle, std::shared_ptr<AppliedTag> right){

        CombinedWith = std::make_tuple(middle, right);
    }

    inline auto GetID() const{

        return ID;
    }

    inline const auto& GetModifiers() const{

        return Modifiers;
    }

    //! \brief Gets the name of the tag used by this AppliedTag
    //! \exception Leviathan::InvalidState if there is no tag
    std::string GetTagName() const;

protected:

    int64_t ID = -1;
    
    //! Primary tag
    std::shared_ptr<Tag> MainTag;
    
    // Modifiers //
    std::vector<std::shared_ptr<TagModifier>> Modifiers;

    //! \brief Combines
    //!
    //! Combined in a form {tag} {word} {tag} Only non-null on the first tag.
    //! The second will have null
    std::tuple<std::string, std::shared_ptr<AppliedTag>> CombinedWith;

};


//! \brief Represents a collection of tags that can be edited
class TagCollection{
public:

    TagCollection(std::vector<std::shared_ptr<AppliedTag>> tags);

    TagCollection();

    bool HasTag(const AppliedTag &tagtocheck);
    
    void Clear();

    //! \brief Removes a tag from this collection if it's id and name match
    bool RemoveTag(const AppliedTag &exacttag);

    //! \brief Removes a tag based on the textual representation of the tag
    bool RemoveText(const std::string &str);

    //! \brief Adds a tag to this collection
    bool Add(std::shared_ptr<Tag> tag);

    //! \brief Adds a tag to this collection
    bool Add(std::shared_ptr<AppliedTag> tag);

    //! \brief Adds tags from other to this collection
    void Add(const TagCollection &other);

    //! \brief Adds all tags from other
    void AddTags(const TagCollection &other);

    //! \brief Replaces all tags with a multiline tag string
    void ReplaceWithText(std::string text);

    //! \brief Converts all tags to text and adds the separator inbetween
    std::string TagsAsString(const std::string &separator);

    
    bool HasTags(){

        CheckIsLoaded();
        return !Tags.empty();
    }

    const auto begin() const{

        return Tags.begin();
    }

    const auto end() const{

        return Tags.end();
    }

    //! Calls _OnCheckTagsLoaded if needed
    inline void CheckIsLoaded(){

        if(TagLoadCheckDone)
            return;

        TagLoadCheckDone = true;
        _OnCheckTagsLoaded();
    }

protected:

    //! Called when retrieving tags, or the tags need to be loaded for some other reason
    virtual void _OnCheckTagsLoaded(){
    }

    //! Callback for easily making child classes
    virtual void _TagRemoved(const AppliedTag &tag){

    }

    //! Callback for child classes
    virtual void _TagAdded(const AppliedTag &tag){

    }

protected:
    
    std::vector<std::shared_ptr<AppliedTag>> Tags;

    bool TagLoadCheckDone = false;
};

//! Allows changing tags in the database with the same interface as TagCollection
class DatabaseTagCollection : public TagCollection{
public:

    DatabaseTagCollection(
        std::function<void (std::vector<std::shared_ptr<AppliedTag>>&)> loadtags,
        std::function<void (const AppliedTag &tag)> onadd,
        std::function<void (const AppliedTag &tag)> onremove) :
        OnAddTag(onadd),
        OnRemoveTag(onremove),
        LoadTags(loadtags)
    {
    }

protected:

    //! Applies the change to the database
    void _TagRemoved(const AppliedTag &tag) override{
        
        OnRemoveTag(tag);
    }

    //! Applies the change to the database
    void _TagAdded(const AppliedTag &tag) override{
        
        OnAddTag(tag);
    }

    //! Used to load tags from the database
    void _OnCheckTagsLoaded() override{

        if(TagsLoaded)
            return;

        TagsLoaded = true;

        // Load tags from the database //
        LoadTags(Tags);
    }
    
protected:

    std::function<void (const AppliedTag &tag)> OnAddTag;
    std::function<void (const AppliedTag &tag)> OnRemoveTag;

    std::function<void (std::vector<std::shared_ptr<AppliedTag>>&)> LoadTags;
    bool TagsLoaded = false;
};



}
    
