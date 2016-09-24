#pragma once

#include "core/DualView.h"

namespace DV{

//! \brief Version of DualView that works well for tests
class TestDualView : public DualView{
public:

    TestDualView() : DualView(true){

        
    }

};

}
