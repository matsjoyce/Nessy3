#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"
#include "thunk.hpp"

class TestThunk;
class GetThunk;
class SetThunk;

using DollarName = std::vector<std::string>;

struct ExecutionState {
    std::vector<std::shared_ptr<const TestThunk>> test_thunks;
    std::multimap<std::shared_ptr<const Thunk>, std::shared_ptr<const Thunk>> thunk_subscriptions;
    std::vector<std::pair<std::shared_ptr<const Thunk>, BaseObjectRef>> thunk_results;
    std::multimap<DollarName, std::shared_ptr<const GetThunk>> get_thunks;
    std::multimap<DollarName, std::shared_ptr<const SetThunk>> set_thunks;
    std::map<DollarName, ObjectRef> dollar_values;
    std::vector<DollarName> resolution_order;
};


class ExecutionEngine {
    std::map<std::string, ObjectRef> env_additions;
    std::map<std::string, ObjectRef> modules;
    std::multimap<DollarName, DollarName> ordering;
    ExecutionState state, initial_state;

    BaseObjectRef test_thunk(std::string name);
    ObjectRef import_(std::string name);
    BaseObjectRef dollar_get(DollarName name, unsigned int flags);
    BaseObjectRef dollar_set(DollarName, ObjectRef value, unsigned int flags);

    void resolve_dollar(DollarName);
    void notify_thunks();
    void check_consistency();
    DollarName pick_next_dollar_name();
public:
    ExecutionEngine();
    static TypeRef type;
    void finish();
    void exec_file(std::string fname);
    void exec_runspec(ObjectRef runspec);
    void subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest);
    void finalize_thunk(std::shared_ptr<const Thunk> source, BaseObjectRef result);
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(ExecutionEngine* execengine, std::string name);
    std::string to_str() const override;
};

class GetThunk : public Thunk {
    DollarName name;
    unsigned int flags;
public:
    GetThunk(ExecutionEngine* execengine, DollarName name, unsigned int flags);
    std::string to_str() const override;

    friend class ExecutionEngine;
};

class SetThunk : public Thunk {
    DollarName name;
    ObjectRef value;
    unsigned int flags;
public:
    SetThunk(ExecutionEngine* execengine, DollarName name, ObjectRef value, unsigned int flags);
    std::string to_str() const override;

    friend class ExecutionEngine;
};

#endif // EXECUTIONENGINE_HPP
