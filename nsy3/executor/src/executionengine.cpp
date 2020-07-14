#include "executionengine.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "frame.hpp"
#include "builtins.hpp"

#include <iostream>

ExecutionEngine::ExecutionEngine() {
    env_additions = {
        {"test_thunk", create<BuiltinFunction>(std::function<ObjectRef(std::string)>(std::bind(&ExecutionEngine::test_thunk, this, std::placeholders::_1)))},
        {"import", create<BuiltinFunction>(std::function<ObjectRef(std::string)>(std::bind(&ExecutionEngine::import_, this, std::placeholders::_1)))}
    };
}

ObjectRef ExecutionEngine::import_(std::string name) {
    return modules.at(name);
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
    c->print(std::cerr);
    auto start_env = builtins;
    auto additions = env_additions;
    start_env.merge(additions);
    std::cerr << "Executing " << fname << std::endl;
    auto frame = create<Frame>(c, 0, start_env);
    auto end_env = frame->execute();
    for (auto& mod : modules) {
        // Startswith
        if (mod.first.rfind(c->modulename() + ".", 0) == 0 && mod.first.rfind(".") == c->modulename().size()) {
            end_env[mod.first.substr(mod.first.rfind(".") + 1)] = mod.second;
        }
    }
    auto module = create<Module>(c->modulename(), end_env);
    modules[c->modulename()] = module;
}

void ExecutionEngine::exec_runspec(ObjectRef runspec) {
    auto runspec_dict = convert_ptr<Dict>(runspec);
    for (auto item : convert_ptr<List>(runspec_dict->get().at(create<String>("files")))->get()) {
        exec_file(convert<std::string>(item));
    }
    std::cerr << "Initial execution done" << std::endl;
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
