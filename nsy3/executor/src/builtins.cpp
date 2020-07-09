#include "builtins.hpp"
#include "builtinfunction.hpp"
#include "bytecode.hpp"
#include "exception.hpp"
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
    return create<Function>(code, offset, signature, env);
}

ObjectRef braks(const std::vector<ObjectRef>& args) {
    return create<List>(args);
}

std::map<std::string, ObjectRef> builtins = {
    {"print", create<BuiltinFunction>(print)},
    {"->", create<BuiltinFunction>(arrow)},
    {"Signature", Signature::type},
    {"[]", create<BuiltinFunction>(braks)},
    {"make_err", create<BuiltinFunction>([]() -> int { throw static_cast<ObjectRef>(create<NameException>("An error.")); })}
};
