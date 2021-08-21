#ifndef EXECUTIONENGINE_HPP
#define EXECUTIONENGINE_HPP

#include "object.hpp"
#include "thunk.hpp"
#include "bytecode.hpp"

class TestThunk;
class GetThunk;
class SetThunk;
class SubThunk;

using DollarName = std::vector<std::string>;
template<class T, class U> using VecMultiMap = std::map<T, std::vector<U>>;

struct ExecutionState {
    std::vector<std::shared_ptr<const TestThunk>> test_thunks;
    VecMultiMap<std::shared_ptr<const Thunk>, std::shared_ptr<const Thunk>> thunk_subscriptions;
    std::map<std::shared_ptr<const Thunk>, BaseObjectRef> thunk_results;
    VecMultiMap<DollarName, std::shared_ptr<const GetThunk>> get_thunks;
    VecMultiMap<DollarName, std::shared_ptr<const SetThunk>> set_thunks;
    VecMultiMap<DollarName, std::shared_ptr<const SubThunk>> sub_thunks;
    VecMultiMap<DollarName, std::string> sub_names;
    std::map<DollarName, ObjectRef> dollar_values;
    std::vector<DollarName> resolution_order;
    std::map<DollarName, DollarName> aliases;
};


class ExecutionEngine {
    std::map<std::string, ObjectRef> env_additions;
    std::map<std::string, BaseObjectRef> modules;
    VecMultiMap<DollarName, DollarName> ordering;
    ExecutionState state, initial_state;
    unsigned int resets = 0;

    BaseObjectRef test_thunk(std::string name);
    BaseObjectRef import_(std::string name);
    BaseObjectRef dollar_get(DollarName name, unsigned int flags);
    BaseObjectRef dollar_set(DollarName, ObjectRef value, unsigned int flags);
    BaseObjectRef make_sub_thunk(DollarName name, unsigned int position);
    void make_alias(DollarName name, DollarName alias);

    void resolve_dollar(DollarName name);
    void resolve_dummy(DollarName name);
    void notify_thunks();
    void check_consistency();
    DollarName pick_next_dollar_name();
    DollarName pick_dummy_name();
    bool finalize_abandoned_get_thunks();
    bool finalize_abandoned_sub_thunks();
public:
    ExecutionEngine();
    static TypeRef type;
    void finish();
    void exec_code(std::shared_ptr<const Code> code);
    void exec_runspec(ObjectRef runspec);
    void subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest);
    void finalize_thunk(std::shared_ptr<const Thunk> source, BaseObjectRef result);
    DollarName dealias(const DollarName& name);

    friend class SubIter;
};

#endif // EXECUTIONENGINE_HPP
