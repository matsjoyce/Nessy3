#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"

class TestThunk;

class ExecutionEngine {
    std::vector<std::shared_ptr<const TestThunk>> test_thunks;
    std::map<std::string, ObjectRef> env_additions;
    std::multimap<std::shared_ptr<const Thunk>, std::shared_ptr<const Thunk>> thunk_subscriptions;
    std::vector<std::pair<std::shared_ptr<const Thunk>, ObjectRef>> thunk_results;
    std::map<std::string, ObjectRef> modules;

    ObjectRef test_thunk(std::string name);
    ObjectRef import_(std::string name);
public:
    ExecutionEngine();
    static TypeRef type;
    void finish();
    void exec_file(std::string fname);
    void exec_runspec(ObjectRef runspec);
    void subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest);
    void finalize_thunk(std::shared_ptr<const Thunk> source, ObjectRef result);
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(TypeRef type, ExecutionEngine* execengine, std::string name);
    std::string to_str() const override;
};

#endif // EXECUTIONENGINE_HPP
