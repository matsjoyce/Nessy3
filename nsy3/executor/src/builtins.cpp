#include "builtins.hpp"
#include "builtinfunction.hpp"
#include "bytecode.hpp"
#include <iostream>

int print(const std::vector<ObjectRef>& args) {
    std::cout << " -> ";
    for (auto& arg : args) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;
    return 0;
}

ObjectRef arrow(std::shared_ptr<Code> code, int offset, std::shared_ptr<Signature> signature, std::map<std::string, ObjectRef> env) {
    return std::make_shared<Function>(code, offset, signature, env);
}

ObjectRef braks(const std::vector<ObjectRef>& args) {
    return std::make_shared<List>(args);
}

std::map<std::string, ObjectRef> builtins = {
    {"print", std::make_shared<BuiltinFunction>(print)},
    {"->", std::make_shared<BuiltinFunction>(arrow)},
    {"Signature", Signature::type},
    {"[]", std::make_shared<BuiltinFunction>(braks)}
};
