#pragma once

#include "DatabaseResource.h"

#include "Common.h"

namespace DV{

class PreparedStatement;

//! see the maintables.sql for more accurate definitions of these
enum class TAG_CATEGORY {
    
    //-- 0 - describes an object or a character in the image
    DESCRIBE_CHARACTER_OBJECT = 0,

    //-- 1 - Tags a character or an person in the image
    CHARACTER = 1,

    //-- 2 - Tags something that's not immediately visible from the image 
    // or relates to something meta, like captions
    META = 2,

    //-- 3 - Tags the series or universe this image belongs to, 
    // for example star wars. Or another loosely defined series
    COPYRIGHT = 3,

    //-- 4 - Tags an action that is being performed
    ACTION = 4,
        
    //-- 5 - The level of quality
    QUALITY_HELPFULL_LEVEL = 5
};

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

    void UpdateProperties(std::string name, std::string category, bool isprivate);

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

    bool operator <(const Tag &other){

        return Name < other.Name;
    }

    void SetName(const std::string &name){

        Name = name;
        OnMarkDirty();
    }

    const auto GetName() const{

        return Name;
    }

    void AddAlias(const std::string alias);

    void RemoveAlias(const std::string alias);

    std::vector<std::shared_ptr<Tag>> GetImpliedTags() const;


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
class AppliedTag : public DatabaseResource{
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

protected:

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


//! \todo When loading from the database load the tags lazily
class TagCollection{
public:

    
        private List<AppliedTag> AllTags = new List<AppliedTag>();


            public TagCollection(List<AppliedTag> fromtags)
    {
        AllTags = fromtags;

        foreach (var tag in AllTags)
        {
            if (tag == null)
                throw new ArgumentException("fromtags had a null value");
        }
    }

            public TagCollection()
    {

    }

            public bool HasTag(AppliedTag tag)
    {
        foreach (var existing in AllTags)
        {
            if (existing.IsSame(tag))
                return true;
        }

        return false;
    }

            public void Clear()
    {
        foreach (var tag in AllTags)
        {
            _TagRemoved(tag);
        }

        AllTags.Clear();
    }

    /// <summary>
    /// Removes a tag from this collection if it matches the reference exactly.
    /// </summary>
    /// <param name="exactobject"></param>
    /// <returns>True if removed false otherwise</returns>
            public bool RemoveTag(AppliedTag exactobject)
    {
        bool removed = AllTags.Remove(exactobject);
        if (removed)
            _TagRemoved(exactobject);

        return removed;
    }

    /// <summary>
    /// Removes at index. Will throw if out of range
    /// </summary>
    /// <param name="index"></param>
            public void RemoveAt(int index)
    {
        var tag = AllTags[index];

        AllTags.RemoveAt(index);
        _TagRemoved(tag);
    }

    /// <summary>
    /// Adds a tag to this collection
    /// </summary>
    /// <param name="tag"></param>
            public bool Add(Tag tag)
    {
        if (tag == null)
            throw new ArgumentException("trying to add tag that is null");

        var toadd = new AppliedTag(tag);

        if (tag == null)
            throw new ArgumentException("failed to create AppliedTag from tag");

        if (HasTag(toadd))
            return false;

        AllTags.Add(toadd);
        _TagAdded(toadd);
        return true;
    }

    /// <summary>
    /// Adds tags from other to this collection
    /// </summary>
    /// <param name="tag"></param>
            public void Add(TagCollection other)
    {
        foreach (AppliedTag tag in other)
        {
            Add(tag);
        }
    }

    /// <summary>
    /// Adds a tag to this collection
    /// </summary>
    /// <param name="tag"></param>
            public bool Add(AppliedTag tag)
    {
        if (tag == null)
            throw new ArgumentException("trying to add tag that is null");

        if (HasTag(tag))
            return false;

        AllTags.Add(tag);
        _TagAdded(tag);
        return true;
    }

    /// <summary>
    /// Replaces all tags with a multiline tag string
    /// </summary>
    /// <param name="textbox"></param>
    /// <param name="DualViewMain"></param>
            public void ReplaceWithText(string textbox, Main DualViewMain)
    {
        string[] lines = textbox.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);

        Clear();

        foreach (var line in lines)
        {
            var tag = DualViewMain.ParseTagFromString(line);
            if (tag == null)
                continue;

            Add(tag);
        }
    }

            public IEnumerator GetEnumerator()
    {
        return AllTags.GetEnumerator();
    }

    internal AppliedTag Get(int index)
    {
        return AllTags[index];
    }

            public bool RemoveText(string str)
    {
        foreach (var tag in AllTags)
        {
            if (tag.ToAccurateString().CompareTo(str) == 0)
            {
                AllTags.Remove(tag);
                _TagRemoved(tag);
                return true;
            }
        }

        return false;
    }

            public string TagsAsString(string separator)
    {
        return String.Join(separator, AllTags.Select(i => i.ToAccurateString().Replace(separator, "\\"+separator)));
    }

    /// <summary>
    /// Callback for easily making child classes
    /// </summary>
    /// <param name="tag"></param>
            protected virtual void _TagRemoved(AppliedTag tag)
    {

    }

    /// <summary>
    /// Callback for child classes
    /// </summary>
    /// <param name="tag"></param>
            protected virtual void _TagAdded(AppliedTag tag)
    {

    }

    void AddTags(const TagCollection &tags){

        
    }
    
    bool HasTags() const{

        return !Tags.empty();
    }

    const auto begin() const{

        return Tags.begin();
    }

    const auto end() const{

        return Tags.end();
    }

protected:

    std::vector<std::shared_ptr<AppliedTag>> Tags;
};

class DatabaseTagCollection : public TagCollection{
public:

    

        public DBTagCollection(List<AppliedTag> fromtags, TagUpdateFunction addTag, TagUpdateFunction removeTag) :
            base(fromtags)
    {
        OnAddTag = addTag;
        OnRemoveTag = removeTag;
    }

            protected override void _TagRemoved(AppliedTag tag)
    {
        OnRemoveTag(tag);
    }

            protected override void _TagAdded(AppliedTag tag)
    {
        OnAddTag(tag);
    }

};



}
    
