#include "object.hpp"
#include "builtinfunction.hpp"

#include <stdexcept>
#include <sstream>

std::ostream& operator<<(std::ostream& s, const ObjectRef& obj) {
    s << obj->to_str();
    return s;
}

Object::Object(std::shared_ptr<Type> type) : type_(type) {
}

ObjectRef Object::type() {
    return type_;
}


ObjectRef Object::getattr(std::string name) {
    auto obj = type_->get(name);
    if (dynamic_cast<BuiltinFunction*>(obj.get())) {
        return std::make_shared<BoundMethod>(shared_from_this(), obj);
    }
    return obj;
}


ObjectRef Object::call(const std::vector<ObjectRef>& args) {
    throw std::runtime_error("Cannot call me");
}



Type::Type() : Object({}) {
}


Type::Type(std::map<std::string, ObjectRef> attrs) : Object({}), attrs(attrs) {
}

std::string Type::to_str() {
    return "Type(?)";
}

ObjectRef Type::get(std::string name) {
    auto iter = attrs.find(name);
    if (iter == attrs.end()) {
        throw std::runtime_error("No such attr");
    }
    return iter->second;
}

auto integer_type = std::make_shared<Type>(Type::attrmap{
    {"+", std::make_shared<BuiltinFunction>([](int a, int b){ return a + b; })},
    {"-", std::make_shared<BuiltinFunction>([](int a, int b){ return a - b; })},
    {"*", std::make_shared<BuiltinFunction>([](int a, int b){ return a * b; })},
    {"/", std::make_shared<BuiltinFunction>([](int a, int b){ return static_cast<double>(a) / b; })},
    {"//", std::make_shared<BuiltinFunction>([](int a, int b){ return a / b; })},
    {"%", std::make_shared<BuiltinFunction>([](int a, int b){ auto x = (a % b); return x > 0 ? x : x + b; })}
});

Integer::Integer(int v) : Numeric(integer_type), value(v) {
}

std::string Integer::to_str() {
    return std::to_string(value);
}

auto float_type = std::make_shared<Type>(Type::attrmap{
    {"+", std::make_shared<BuiltinFunction>([](double a, double b){ return a + b; })},
    {"-", std::make_shared<BuiltinFunction>([](double a, double b){ return a - b; })},
    {"*", std::make_shared<BuiltinFunction>([](double a, double b){ return a * b; })},
    {"/", std::make_shared<BuiltinFunction>([](double a, double b){ return static_cast<double>(a) / b; })},
    {"//", std::make_shared<BuiltinFunction>([](double a, double b){ return static_cast<int>(a / b); })},
//     {"%", std::make_shared<BuiltinFunction>([](double a, double b){ auto x = (a % b); return x > 0 ? x : x + b; })}
});

Float::Float(double v) : Numeric(float_type), value(v) {
}

std::string Float::to_str() {
    return std::to_string(value);
}

auto string_type = std::make_shared<Type>(Type::attrmap{
    {"+", std::make_shared<BuiltinFunction>([](std::string a, std::string b) { return a + b; })},
    {"*", std::make_shared<BuiltinFunction>([](std::string a, int b) {
        std::string res(a.size() * std::max(0, b), '\0');
        for (auto i = 0; i < b; ++i) {
            res.replace(i * a.size(), a.size(), a);
        }
        return res;
    })}
});

String::String(std::string v) : Object(string_type), value(v) {
}

std::string String::to_str() {
    return value;
}

auto bound_method_type = std::make_shared<Type>();

BoundMethod::BoundMethod(ObjectRef self, ObjectRef func) : Object(bound_method_type), self(self), func(func) {
}

std::string BoundMethod::to_str() {
    return "BoundMethod(" + self->to_str() + ", " + func->to_str() + ")";
}

ObjectRef BoundMethod::call(const std::vector<ObjectRef>& args) {
    std::vector<ObjectRef> mod_args = args;
    mod_args.insert(mod_args.begin(), self);
    return func->call(mod_args);
}

auto dict_type = std::make_shared<Type>();

Dict::Dict(ObjectRefMap v) : Object(dict_type), value(v) {
}

std::string Dict::to_str() {
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (auto item : value) {
        if (!first) {
            ss << ", ";
        }
        ss << item.first << ": " << item.second;
        first = false;
    }
    ss << "}";
    return ss.str();
}
