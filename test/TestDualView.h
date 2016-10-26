#pragma once

#include "core/DualView.h"
#include "core/Database.h"

namespace DV{

//! \brief Version of DualView that works well for tests
class TestDualView : public DualView{
public:

    TestDualView(const std::string &dbFile = "") : DualView(true, dbFile){


        
    }

};

//! \brief Version of DualView that has no worker threads
class DummyDualView : public DualView{
public:
    
    DummyDualView() : DualView(std::string("empty")){

        
    }

    DummyDualView(std::unique_ptr<Database> &&db) :
        DualView(std::string("empty"), std::move(db))
    {

        
    }
private:

    void _StartWorkerThreads() override {

    }

    void _WaitForWorkerThreads() override {

    }
};

}
