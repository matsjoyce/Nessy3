#include "builtins.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "exception.hpp"
#include "frame.hpp"
#include <iostream>

int print(const std::vector<ObjectRef>& args) {
    std::cout << " -> ";
    for (auto& arg : args) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;
    return 0;
}

ObjectRef arrow(std::shared_ptr<const Code> code, int offset, std::shared_ptr<const Signature> signature, std::shared_ptr<const Env> env) {
    return create<Function>(code, offset, signature, env->get());
}

ObjectRef braks(const std::vector<ObjectRef>& args) {
    return create<List>(args);
}

bool not_(ObjectRef arg) {
    return !arg->to_bool();
}

int assert(ObjectRef obj) {
    if (!obj->to_bool()) {
        create<AssertionError>("Assertion failed")->raise();
    }
    std::cout << "Assertion passed" << std::endl;
    return 1;
}

std::map<std::string, ObjectRef> builtins = {
    // Types
    {"Object", Object::type},
    {"Float", Float::type},
    {"Integer", Integer::type},
    {"Boolean", Boolean::type},
    {"String", String::type},
    {"Bytes", Bytes::type},
    {"List", List::type},
    {"Dict", Dict::type},

    // Consts
    {"TRUE", Boolean::true_},
    {"FALSE", Boolean::false_},
    {"NONE", NoneType::none},

    // Functions
    {"print", create<BuiltinFunction>(print)},
    {"->", create<BuiltinFunction>(arrow)},
    {"Signature", Signature::type},
    {"[]", create<BuiltinFunction>(braks)},
    {"assert", create<BuiltinFunction>(assert)},
    {"not", create<BuiltinFunction>(not_)},

};
