#include "builtins.hpp"
#include "functionutils.hpp"
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

ObjectRef arrow(std::shared_ptr<const Code> code, int offset, std::shared_ptr<const Signature> signature, std::map<std::string, ObjectRef> env) {
    return create<Function>(code, offset, signature, env);
}

ObjectRef braks(const std::vector<ObjectRef>& args) {
    return create<List>(args);
}

bool not_(ObjectRef arg) {
    return !arg->to_bool();
}

int assert(ObjectRef obj) {
    if (!obj->to_bool()) {
        create<AssertionException>("Assertion failed")->raise();
    }
    std::cout << "Assertion passed" << std::endl;
    return 1;
}

std::map<std::string, ObjectRef> builtins = {
    {"print", create<BuiltinFunction>(print)},
    {"->", create<BuiltinFunction>(arrow)},
    {"Signature", Signature::type},
    {"[]", create<BuiltinFunction>(braks)},
    {"assert", create<BuiltinFunction>(assert)},
    {"not", create<BuiltinFunction>(not_)},
    {"Float", Float::type},
    {"Integer", Integer::type},
    {"Object", Object::type}
};
