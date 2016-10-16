#pragma once

#include "DatabaseResource.h"

namespace DV{




class Tag : public DatabaseResource{
public:

    

};

class AppliedTag {
public:

    std::string FormatAsString() const;

};


class TagCollection{
public:


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

class DatabaseTagCollection : public DatabaseResource, public TagCollection{
public:

    

};



}
    
