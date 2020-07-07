#include "builtins.hpp"
#include "builtinfunction.hpp"
#include "bytecode.hpp"
#include <iostream>

int print(ObjectRef x) {
    std::cout << " -> " << x << std::endl;
    return 0;
}

ObjectRef arrow(std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env) {
    return std::make_shared<Function>(code, offset, env);
}

std::map<std::string, ObjectRef> builtins = {
    {"print", std::make_shared<BuiltinFunction>(print)},
    {"->", std::make_shared<BuiltinFunction>(arrow)}
};
