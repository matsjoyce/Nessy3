#include "executionengine.hpp"
#include "builtinfunction.hpp"
#include <iostream>

auto execution_engine_type = std::make_shared<Type>(Type::attrmap{
    {"test_thunk", std::make_shared<BuiltinFunction>(method(&ExecutionEngine::test_thunk))}
});

ExecutionEngine::ExecutionEngine() : Object(execution_engine_type) {
}

std::string ExecutionEngine::to_str() {
    return "ExecutionEngine";
}

ObjectRef ExecutionEngine::test_thunk(std::string name) {
    auto tt = std::make_shared<TestThunk>(name);
    test_thunks.push_back(tt);
    return tt;
}

void ExecutionEngine::finish() {
    while (test_thunks.size()) {
        auto tt = test_thunks.back();
        test_thunks.pop_back();
        std::cout << "Resolving " << tt->to_str() << std::endl;
        tt->finalize(std::make_shared<Integer>(1));
    }
}


TestThunk::TestThunk(std::string name) : name(name) {
}

std::string TestThunk::to_str() {
    return "TestThunk(" + name + ")";
}
