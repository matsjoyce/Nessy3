#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"

class TestThunk;

class ExecutionEngine : public Object {
    std::vector<std::shared_ptr<TestThunk>> test_thunks;
public:
    ExecutionEngine();
    std::string to_str() override;
    ObjectRef test_thunk(std::string name);
    void finish();
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(std::string name);
    std::string to_str() override;
};

#endif // EXECUTIONENGINE_HPP
