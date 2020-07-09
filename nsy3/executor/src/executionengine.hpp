#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"

class TestThunk;

class ExecutionEngine : public Object {
    std::vector<std::shared_ptr<TestThunk>> test_thunks;
public:
    ExecutionEngine(TypeRef type);
    std::string to_str() override;
    static TypeRef type;
    ObjectRef test_thunk(std::string name);
    void finish();
    void exec_file(std::string fname);
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(TypeRef type, std::string name);
    std::string to_str() override;
};

#endif // EXECUTIONENGINE_HPP
