#include "script.h"

#include <iostream>

class DerivedScript : public Script {

public:
    virtual void execute() override {
        std::cout << "called execute() from .so" << std::endl;
    }
};

extern "C" Script* createScript() {
    return new DerivedScript;
}