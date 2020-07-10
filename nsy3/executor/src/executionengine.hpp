#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"

class TestThunk;

class ExecutionEngine {
    std::vector<std::shared_ptr<const TestThunk>> test_thunks;
    std::map<std::string, ObjectRef> env_additions;
public:
    ExecutionEngine();
    static TypeRef type;
    ObjectRef test_thunk(std::string name);
    void finish();
    void exec_file(std::string fname);
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(TypeRef type, std::string name);
    std::string to_str() const override;
};

#endif // EXECUTIONENGINE_HPP
