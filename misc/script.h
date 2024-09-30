#pragma once

class Script {

public:
    virtual void execute() = 0;

// protected:
//     virtual ~Script() {}

};

// extern "C" Script* createScript();