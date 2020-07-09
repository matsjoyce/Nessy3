#include "executionengine.hpp"
#include "builtinfunction.hpp"
#include "bytecode.hpp"
#include "frame.hpp"
#include "builtins.hpp"

#include <iostream>

TypeRef ExecutionEngine::type = create<Type>("ExecutionEngine", Type::attrmap{
    {"test_thunk", create<BuiltinFunction>(method(&ExecutionEngine::test_thunk))}
});

ExecutionEngine::ExecutionEngine(TypeRef type) : Object(type) {
}

std::string ExecutionEngine::to_str() {
    return "ExecutionEngine";
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
    start_env["__exec__"] = shared_from_this();
    create<Frame>(c, 0, start_env)->execute();
}

TestThunk::TestThunk(TypeRef type, std::string name) : Thunk(type), name(name) {
}

std::string TestThunk::to_str() {
    return "TestThunk(" + name + ")";
}
