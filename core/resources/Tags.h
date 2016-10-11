#pragma once

#include "DatabaseResource.h"

namespace DV{




class Tag : public DatabaseResource{
public:

    

};


class TagCollection{
public:


    void AddTags(const TagCollection &tags){

        
    }
    
    bool HasTags() const{

        return !Tags.empty();
    }

protected:

    std::vector<Tag> Tags;
};

class DatabaseTagCollection : public DatabaseResource, public TagCollection{
public:

    

};



}
    
