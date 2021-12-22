#include "builtins.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "exception.hpp"
#include "frame.hpp"
#include <iostream>

ObjectRef print(const std::vector<ObjectRef>& args) {
    std::cout << " -> ";
    for (auto& arg : args) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;
    return NoneType::none;
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

ObjectRef assert(ObjectRef obj) {
    if (!obj->to_bool()) {
        create<AssertionError>("Assertion failed")->raise();
    }
    std::cout << "Assertion passed" << std::endl;
    return NoneType::none;
}

class Range : public Object {
    int start_, stop_;
public:
    Range(TypeRef type, int start, int stop) : Object(type), start_(start), stop_(stop) {}
    std::string to_str() const override {
        return "Range(" + std::to_string(start_) +  ", " + std::to_string(stop_) + ")";
    }
    static TypeRef type;
    ObjectRef next() const {
        if (start_ >= stop_) {
            return NoneType::none;
        }
        else {
            return create<List>(std::vector<ObjectRef>{create<Range>(start_ + 1, stop_), create<Integer>(start_)});
        }
    }
};

TypeRef Range::type = create<Type>("Range", Type::basevec{Object::type}, Type::attrmap{
    {"__iter__", create<BuiltinFunction>([](ObjectRef self) -> ObjectRef {
        return self;
    })},
    {"__next__", create<BuiltinFunction>(method(&Range::next))},
    {"__new__", create<BuiltinFunction>([](int start, int stop) -> ObjectRef {
        return create<Range>(start, stop);
    })}
});

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
    {"Range", Range::type},

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
