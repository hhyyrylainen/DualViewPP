#pragma once

#include "DatabaseResource.h"

#include "Common.h"
#include <functional>

namespace DV {

class PreparedStatement;

//! \brief Represents a word that is placed before tag, like "red flower"
//! \note When deleted AppliedTags need to be scanned for this (the database has RESTRICT on
//! this) and if found, removed. Then if that results in duplicate AppliedTags that needs to be
//! handled
class TagModifier : public DatabaseResource {
public:
    TagModifier(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~TagModifier();

    std::string ToAccurateString()
    {
        return Name;
    }

    bool operator==(const TagModifier& other) const
    {
        return Name == other.Name;
    }

    const auto GetName() const
    {
        return Name;
    }

    const auto GetDescription() const
    {
        return Description;
    }

    const auto GetIsPrivate() const
    {
        return IsPrivate;
    }

    bool IsDeleted() const
    {
        return Deleted;
    }

    void UpdateProperties(std::string name, std::string category, bool isprivate);

protected:
    void _DoSave(Database& db) override;

protected:
    std::string Name;
    bool IsPrivate;
    std::string Description;
    bool Deleted = false;
};

//! Holds data that Tag has, used to create non-database tag objects
class TagData {
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
class Tag : public DatabaseResource {
public:
    //! Can be used to mark tags as selected, used at least for exporting downloaded tags
    bool Selected = false;

    //! Creates a tag for adding to the database
    Tag(std::string name, std::string description, TAG_CATEGORY category,
        bool isprivate = false);

    Tag(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~Tag();

    bool operator<(const Tag& other) const
    {
        return Name < other.Name;
    }

    bool operator!=(const Tag& other) const
    {
        return !(*this == other);
    }

    bool operator==(const Tag& other) const;

    void SetName(const std::string& name)
    {
        Name = name;
        OnMarkDirty();
    }

    const auto GetName() const
    {
        return Name;
    }


    const auto GetCategory() const
    {
        return Category;
    }

    void SetCategory(TAG_CATEGORY category)
    {
        Category = category;
        OnMarkDirty();
    }

    const auto GetDescription() const
    {
        return Description;
    }

    void SetDescription(const std::string& newdescription)
    {
        Description = newdescription;
        OnMarkDirty();
    }

    const auto GetIsPrivate() const
    {
        return IsPrivate;
    }

    void SetIsPrivate(bool newvalue)
    {
        IsPrivate = newvalue;
        OnMarkDirty();
    }

    bool IsDeleted() const
    {
        return Deleted;
    }

    void AddAlias(const std::string alias);

    void RemoveAlias(const std::string alias);

    std::vector<std::string> GetAliases() const;


    std::vector<std::shared_ptr<Tag>> GetImpliedTags() const;

    void AddImpliedTag(std::shared_ptr<Tag> imply);

    void RemoveImpliedTag(std::shared_ptr<Tag> imply);


protected:
    void _DoSave(Database& db) override;

protected:
    //! The text of the tag
    std::string Name;

    //! Description string of the tag
    std::string Description;

    bool IsPrivate;
    TAG_CATEGORY Category = TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT;

    bool Deleted = false;
};


//! Represents an imply relationship between two tags
//! When the ImpliedBy tag is applied also the Primary tag should be assumed to be applied
//! \todo Allow loading and saving these from the database
class ImpliedTag {
public:
    ImpliedTag(std::shared_ptr<Tag> tag, std::shared_ptr<Tag> impliedby) :
        /*DatabaseResource(false),*/
        Primary(tag), ImpliedBy(impliedby)
    {}

    std::string GetImplySqlComment()
    {
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

        if(comment)
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
    TagBreakRule(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~TagBreakRule();

    //! Breaks a string according to this rule
    //! \returns The modifiers that were before the tag
    //! \param tagname If break succeeds this contains the name of the tag
    //! \param returnedtag If break succeeds this contains the tag
    //! \todo This needs to be worked entirely
    std::vector<std::shared_ptr<TagModifier>> DoBreak(
        std::string str, std::string& tagname, std::shared_ptr<Tag>& returnedtag);

    void UpdateProperties(
        std::string newpattern, std::string newmaintag, std::vector<std::string> newmodifiers);

protected:
    void _DoSave(Database& db) override;

protected:
    std::string Pattern;
    std::shared_ptr<Tag> ActualTag;
    std::vector<std::shared_ptr<TagModifier>> Modifiers;
};




//! A full tag that is applied to something
//! \note Changes to this object will not be saved to the database. If you want to change
//! an AppliedTag in the database: first remove it and then add the new one
class AppliedTag {
    friend Database;

public:
    //! Creates an applied tag for a tag
    AppliedTag(std::shared_ptr<Tag> tagonly);

    AppliedTag(std::shared_ptr<Tag> tag, std::vector<std::shared_ptr<TagModifier>> modifiers);

    AppliedTag(std::tuple<std::vector<std::shared_ptr<TagModifier>>, std::shared_ptr<Tag>>
            modifiersandtag);

    //! \brief Creates a combined tag with a string in between
    //! \note Implicitly creates a new applied tag from the second tag
    AppliedTag(std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<Tag>> composite);

    //! Creates a composite with an existing right hand side of the composite already created
    AppliedTag(
        std::tuple<std::shared_ptr<Tag>, std::string, std::shared_ptr<AppliedTag>> composite);

    AppliedTag(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    //! Alias for IsSame
    bool operator==(const AppliedTag& other) const
    {
        return IsSame(other);
    }

    bool IsSame(const AppliedTag& other) const;

    //! Creates a string representation that can be parsed with DualView::ParseTagFromString
    std::string ToAccurateString() const;

    //! \brief Returns true if CombinedWith is set. And returns the values in the parameters
    bool GetCombinedWith(std::string& combinestr, std::shared_ptr<AppliedTag>& combined) const;

    //! \brief Sets the combine with
    //! \todo Assert if called on one loaded from database
    void SetCombineWith(const std::string& middle, std::shared_ptr<AppliedTag> right);

    //! \brief Sets the modifiers on this tag
    void SetModifiers(std::vector<std::shared_ptr<TagModifier>> modifiers)
    {

        Modifiers = modifiers;
    }

    inline auto GetID() const
    {
        return ID;
    }

    inline const auto& GetModifiers() const
    {
        return Modifiers;
    }

    inline const auto GetTag() const
    {
        return MainTag;
    }

    //! \returns True if any of the tags involved with this AppliedTag are deleted. This should
    //! be hidden from the GUI.
    bool HasDeletedParts() const
    {
        if(MainTag->IsDeleted())
            return true;

        for(const auto& modifier : Modifiers)
            if(modifier->IsDeleted())
                return true;

        if(std::get<1>(CombinedWith))
            return std::get<1>(CombinedWith)->HasDeletedParts();

        return false;
    }

    //! \brief Gets the name of the tag used by this AppliedTag
    //! \exception Leviathan::InvalidState if there is no tag
    std::string GetTagName() const;

protected:
    //! Database has abandoned us
    void Orphaned()
    {
        ID = -1;
    }

    //! Whe have been added to the database
    void Adopt(DBID id)
    {
        ID = id;
    }

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
class TagCollection {
public:
    TagCollection(std::vector<std::shared_ptr<AppliedTag>> tags);

    TagCollection();

    bool HasTag(const AppliedTag& tagtocheck);

    void Clear();

    //! \brief Removes a tag from this collection if it's id and name match
    bool RemoveTag(const AppliedTag& exacttag);

    //! \brief Removes a tag based on the textual representation of the tag
    bool RemoveText(const std::string& str);

    //! \brief Adds a tag to this collection
    bool Add(std::shared_ptr<Tag> tag);


    //! \brief Adds a tag to this collection
    bool Add(std::shared_ptr<AppliedTag> tag);

    //! \brief Adds a tag to this collection. Uses existing db lock
    bool Add(std::shared_ptr<AppliedTag> tag, DatabaseLockT& dblock);


    //! \brief Adds tags from other to this collection
    void Add(const TagCollection& other);

    //! \brief Adds all tags from other. Uses existing db lock
    void Add(const TagCollection& other, DatabaseLockT& dblock);

    //! \brief Replaces all tags with a multiline tag string
    void ReplaceWithText(std::string text);

    //! \brief Replaces all tags with a tag string with specific separator
    void ReplaceWithText(const std::string& text, const std::string& separator);

    //! \brief Parses tags from a string with separators and adds them
    //!
    //! This ignores invalid tags. If you need to handle them you need to manually parse
    //! them and call Add
    void AddTextTags(const std::string& text, const std::string& separator);

    //! \brief Converts all tags to text and adds the separator inbetween
    std::string TagsAsString(const std::string& separator);

    //! \brief Returns the number of tags set
    size_t GetTagCount();

    bool HasTags()
    {
        CheckIsLoaded();
        return !Tags.empty();
    }

    bool HasTags(DatabaseLockT& databaselock)
    {
        CheckIsLoaded(databaselock);
        return !Tags.empty();
    }

    const auto begin() const
    {
        return Tags.begin();
    }

    const auto end() const
    {
        return Tags.end();
    }

    //! \brief Reloads tags if this is loaded from the database
    virtual void RefreshTags() {}

    //! Calls _OnCheckTagsLoaded if needed
    inline void CheckIsLoaded()
    {
        if(TagLoadCheckDone)
            return;

        TagLoadCheckDone = true;
        _OnCheckTagsLoaded();
    }

    inline void CheckIsLoaded(DatabaseLockT& dblock)
    {
        if(TagLoadCheckDone)
            return;

        TagLoadCheckDone = true;
        _OnCheckTagsLoaded(dblock);
    }

protected:
    //! Called when retrieving tags, or the tags need to be loaded for some other reason
    virtual void _OnCheckTagsLoaded() {}

    //! Called when retrieving tags, or the tags need to be loaded for
    //! some other reason. This version is used when db is already
    //! locked
    virtual void _OnCheckTagsLoaded(DatabaseLockT& dblock) {}

    //! Callback for easily making child classes
    virtual void _TagRemoved(AppliedTag& tag) {}

    //! Callback for child classes
    virtual void _TagAdded(AppliedTag& tag) {}

    //! Callback for child classes. This version is used when db is already locked
    virtual void _TagAdded(AppliedTag& tag, DatabaseLockT& dblock) {}

protected:
    std::vector<std::shared_ptr<AppliedTag>> Tags;

    bool TagLoadCheckDone = false;
};

//! Allows changing tags in the database with the same interface as TagCollection
//! \todo Make the functions return a success value to catch bugs with dead weak_ptrs
class DatabaseTagCollection : public TagCollection {
public:
    DatabaseTagCollection(
        std::function<void(DatabaseLockT& dblock, std::vector<std::shared_ptr<AppliedTag>>&)>
            loadtags,
        std::function<void(DatabaseLockT& dblock, AppliedTag& tag)> onadd,
        std::function<void(DatabaseLockT& dblock, AppliedTag& tag)> onremove, Database& db) :
        OnAddTag(onadd),
        OnRemoveTag(onremove), LoadTags(loadtags), LoadedDB(db)
    {}

    //! \brief Reloads tags from the database
    void RefreshTags() override;

protected:
    //! Applies the change to the database
    void _TagRemoved(AppliedTag& tag) override;

    //! Applies the change to the database
    void _TagAdded(AppliedTag& tag) override;

    void _TagAdded(AppliedTag& tag, DatabaseLockT& dblock) override;

    //! Used to load tags from the database
    void _OnCheckTagsLoaded() override;

    void _OnCheckTagsLoaded(DatabaseLockT& dblock) override;

protected:
    std::function<void(DatabaseLockT& dblock, AppliedTag& tag)> OnAddTag;
    std::function<void(DatabaseLockT& dblock, AppliedTag& tag)> OnRemoveTag;

    std::function<void(DatabaseLockT& dblock, std::vector<std::shared_ptr<AppliedTag>>&)>
        LoadTags;

    Database& LoadedDB;

    bool TagsLoaded = false;
};



} // namespace DV
