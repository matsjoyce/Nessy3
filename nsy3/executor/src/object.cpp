#include "object.hpp"
#include "builtinfunction.hpp"

#include <stdexcept>
#include <sstream>
#include <iostream>

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
    if (dynamic_cast<Property*>(obj.get())) {
        return obj->call({shared_from_this()});
    }
    return obj;
}

ObjectRef Object::call(const std::vector<ObjectRef>& args) {
    throw std::runtime_error("Cannot call me");
}

bool Object::to_bool() {
    return true;
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

ObjectRef Type::call(const std::vector<ObjectRef>& args) {
    return get("__new__")->call(args);
}

auto integer_type = std::make_shared<Type>(Type::attrmap{
    {"+", std::make_shared<BuiltinFunction>([](int a, int b) { return a + b; })},
    {"-", std::make_shared<BuiltinFunction>([](int a, int b) { return a - b; })},
    {"*", std::make_shared<BuiltinFunction>([](int a, int b) { return a * b; })},
    {"/", std::make_shared<BuiltinFunction>([](int a, int b) { return static_cast<double>(a) / b; })},
    {"//", std::make_shared<BuiltinFunction>([](int a, int b) { return a / b; })},
    {"%", std::make_shared<BuiltinFunction>([](int a, int b) { auto x = (a % b); return x > 0 ? x : x + b; })},
    {"==", std::make_shared<BuiltinFunction>([](int a, ObjectRef b) {
        if (auto obj = dynamic_cast<Integer*>(b.get())) return a == obj->get();
        return false;
    })},
    {"<", std::make_shared<BuiltinFunction>([](int a, int b) { return a < b; })},
});

Integer::Integer(int v) : Numeric(integer_type), value(v) {
}

Integer::Integer(std::shared_ptr<Type> type, int v) : Numeric(type), value(v) {
}

std::string Integer::to_str() {
    return std::to_string(value);
}

bool Integer::to_bool() {
    return value;
}

auto boolean_type = std::make_shared<Type>();

Boolean::Boolean(bool v) : Integer(boolean_type, v) {
}

std::string Boolean::to_str() {
    return value ? "TRUE" : "FALSE";
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

bool Float::to_bool() {
    return value;
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

auto property_type = std::make_shared<Type>();

Property::Property(ObjectRef func) : Object(property_type), func(func) {
}

std::string Property::to_str() {
    return "Property(" + func->to_str() + ")";
}

ObjectRef Property::call(const std::vector<ObjectRef>& args) {
    return func->call(args);
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

auto list_type = std::make_shared<Type>();

List::List(std::vector<ObjectRef> v) : Object(list_type), value(v) {
}

std::string List::to_str() {
    std::stringstream ss;
    ss << "[";
    bool first = true;
    for (auto item : value) {
        if (!first) {
            ss << ", ";
        }
        ss << item;
        first = false;
    }
    ss << "]";
    return ss.str();
}

auto thunk_type = std::make_shared<Type>();

Thunk::Thunk() : Object(thunk_type) {
}

std::string Thunk::to_str() {
    return "Thunk";
}

Thunk::~Thunk() {
    if (!finalized) {
        std::cout << "Thunk was not finalized!" << std::endl;
    }
}

void Thunk::subscribe(std::shared_ptr<Thunk> thunk) {
    waiting_thunks.push_back(thunk);
}

void Thunk::notify(ObjectRef obj) {
}

void Thunk::finalize(ObjectRef obj) {
    for (auto thunk : waiting_thunks) {
        thunk->notify(obj);
    }
    finalized = true;
}
