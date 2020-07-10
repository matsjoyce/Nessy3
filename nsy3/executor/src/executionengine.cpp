#include "executionengine.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "frame.hpp"
#include "builtins.hpp"

#include <iostream>

ExecutionEngine::ExecutionEngine() {
    env_additions = {
        {"test_thunk", create<BuiltinFunction>(std::function<ObjectRef(std::string)>(std::bind(&ExecutionEngine::test_thunk, this, std::placeholders::_1)))}
    };
}

ObjectRef ExecutionEngine::test_thunk(std::string name) {
    auto tt = create<TestThunk>(this, name);
    test_thunks.push_back(tt);
    return tt;
}

void ExecutionEngine::finish() {
    while (test_thunks.size() || thunk_results.size()) {
        while (test_thunks.size()) {
            auto tt = test_thunks.back();
            test_thunks.pop_back();
            tt->finalize(create<Integer>(1));
        }
        while (thunk_results.size()) {
            auto [thunk, res] = thunk_results.back();
            thunk_results.pop_back();
            auto subbed = thunk_subscriptions.equal_range(thunk);
            for (auto iter = subbed.first; iter != subbed.second; ++iter) {
                iter->second->notify(res);
            }
        }
    }
}

void ExecutionEngine::exec_file(std::string fname) {
    auto c = Code::from_file(fname);
    c->print();
    auto start_env = builtins;
    auto additions = env_additions;
    start_env.merge(additions);
    create<Frame>(c, 0, start_env)->execute();
}

void ExecutionEngine::subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest) {
    thunk_subscriptions.insert({source, dest});
}

void ExecutionEngine::finalize_thunk(std::shared_ptr<const Thunk> source, ObjectRef result) {
    thunk_results.push_back({source, result});
}

TestThunk::TestThunk(TypeRef type, ExecutionEngine* execengine, std::string name) : Thunk(type, execengine), name(name) {
}

std::string TestThunk::to_str() const {
    return "TT(" + name + ")";
}
