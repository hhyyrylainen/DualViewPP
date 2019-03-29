#pragma once

namespace DV {

class Database;

//! \brief Handles parsing text form tags
//!
//! This is a class in order to not have to pass the database to all the functions and having
//! this in DualView is not very nice.
class TagParser {
public:
    TagParser(Database& db);


    static TagParser* Get();

private:
    Database& DB;

    static TagParser* StaticAccess;
};
} // namespace DV
