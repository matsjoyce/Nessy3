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
    auto tt = create<TestThunk>(name);
    test_thunks.push_back(tt);
    return tt;
}

void ExecutionEngine::finish() {
    while (test_thunks.size()) {
        auto tt = test_thunks.back();
        test_thunks.pop_back();
        std::cout << "Resolving " << tt->to_str() << std::endl;
        tt->finalize(create<Integer>(1));
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

TestThunk::TestThunk(TypeRef type, std::string name) : Thunk(type), name(name) {
}

std::string TestThunk::to_str() const {
    return "TT(" + name + ")";
}
