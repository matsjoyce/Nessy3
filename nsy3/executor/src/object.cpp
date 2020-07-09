#include "object.hpp"
#include "builtinfunction.hpp"

#include <stdexcept>
#include <sstream>
#include <iostream>

std::vector<std::pair<std::weak_ptr<Object>, TypeRef*>> untyped_objects;

std::ostream& operator<<(std::ostream& s, const ObjectRef& obj) {
    if (!obj) {
        s << "NULL OBJECT!!!";
    }
    else {
        s << obj->to_str();
    }
    return s;
}

Object::Object(TypeRef type) : type_(type) {
}

ObjectRef Object::obj_type() {
    return type_;
}

ObjectRef Object::getattr(std::string name) {
    auto obj = type_->get(name);
    if (dynamic_cast<BuiltinFunction*>(obj.get())) {
        return create<BoundMethod>(shared_from_this(), obj);
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

TypeRef Type::make_type_type() {
    auto obj = create<Type>();
    obj->type_ = obj;
    return obj;
}

TypeRef Type::type = Type::make_type_type();//create<Type>();

Type::Type(TypeRef type, std::map<std::string, ObjectRef> attrs) : Object(type), attrs(attrs) {
    for (auto iter = untyped_objects.begin(); iter != untyped_objects.end();) {
        if (iter->first.expired()) {
            iter = untyped_objects.erase(iter);
        }
        else if (*(iter->second)) {
            iter->first.lock()->type_ = *(iter->second);
            iter = untyped_objects.erase(iter);
        }
        else {
            ++iter;
        }
    }
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

TypeRef Integer::type = create<Type>(Type::attrmap{
    {"+", create<BuiltinFunction>([](int a, int b) { return a + b; })},
    {"-", create<BuiltinFunction>([](int a, int b) { return a - b; })},
    {"*", create<BuiltinFunction>([](int a, int b) { return a * b; })},
    {"/", create<BuiltinFunction>([](int a, int b) { return static_cast<double>(a) / b; })},
    {"//", create<BuiltinFunction>([](int a, int b) { return a / b; })},
    {"%", create<BuiltinFunction>([](int a, int b) { auto x = (a % b); return x > 0 ? x : x + b; })},
    {"==", create<BuiltinFunction>([](int a, ObjectRef b) {
        if (auto obj = dynamic_cast<Integer*>(b.get())) return a == obj->get();
        return false;
    })},
    {"<", create<BuiltinFunction>([](int a, int b) { return a < b; })},
});

Integer::Integer(TypeRef type, int v) : Numeric(type), value(v) {
}

std::string Integer::to_str() {
    return std::to_string(value);
}

bool Integer::to_bool() {
    return value;
}

TypeRef Boolean::type = create<Type>();

Boolean::Boolean(TypeRef type, bool v) : Integer(type, v) {
}

std::string Boolean::to_str() {
    return value ? "TRUE" : "FALSE";
}

TypeRef Float::type = create<Type>(Type::attrmap{
    {"+", create<BuiltinFunction>([](double a, double b){ return a + b; })},
    {"-", create<BuiltinFunction>([](double a, double b){ return a - b; })},
    {"*", create<BuiltinFunction>([](double a, double b){ return a * b; })},
    {"/", create<BuiltinFunction>([](double a, double b){ return static_cast<double>(a) / b; })},
    {"//", create<BuiltinFunction>([](double a, double b){ return static_cast<int>(a / b); })},
//     {"%", create<BuiltinFunction>([](double a, double b){ auto x = (a % b); return x > 0 ? x : x + b; })}
});

Float::Float(TypeRef type, double v) : Numeric(type), value(v) {
}

std::string Float::to_str() {
    return std::to_string(value);
}

bool Float::to_bool() {
    return value;
}

TypeRef String::type = create<Type>(Type::attrmap{
    {"+", create<BuiltinFunction>([](std::string a, std::string b) { return a + b; })},
    {"*", create<BuiltinFunction>([](std::string a, int b) {
        std::string res(a.size() * std::max(0, b), '\0');
        for (auto i = 0; i < b; ++i) {
            res.replace(i * a.size(), a.size(), a);
        }
        return res;
    })}
});

String::String(TypeRef type, std::string v) : Object(type), value(v) {
}

std::string String::to_str() {
    return value;
}
TypeRef Bytes::type = create<Type>(Type::attrmap{
    {"+", create<BuiltinFunction>([](std::string a, std::string b) { return a + b; })},
    {"*", create<BuiltinFunction>([](std::string a, int b) {
        std::string res(a.size() * std::max(0, b), '\0');
        for (auto i = 0; i < b; ++i) {
            res.replace(i * a.size(), a.size(), a);
        }
        return res;
    })}
});

Bytes::Bytes(TypeRef type, std::basic_string<unsigned char> v) : Object(type), value(v) {
}

std::string Bytes::to_str() {
    return "Bytes";
}

TypeRef BoundMethod::type = create<Type>();

BoundMethod::BoundMethod(TypeRef type, ObjectRef self, ObjectRef func) : Object(type), self(self), func(func) {
}

std::string BoundMethod::to_str() {
    return "BoundMethod(" + self->to_str() + ", " + func->to_str() + ")";
}

ObjectRef BoundMethod::call(const std::vector<ObjectRef>& args) {
    std::vector<ObjectRef> mod_args = args;
    mod_args.insert(mod_args.begin(), self);
    return func->call(mod_args);
}

TypeRef Property::type = create<Type>();

Property::Property(TypeRef type, ObjectRef func) : Object(type), func(func) {
}

std::string Property::to_str() {
    return "Property(" + func->to_str() + ")";
}

ObjectRef Property::call(const std::vector<ObjectRef>& args) {
    return func->call(args);
}

TypeRef Dict::type = create<Type>();

Dict::Dict(TypeRef type, ObjectRefMap v) : Object(type), value(v) {
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

TypeRef List::type = create<Type>();

List::List(TypeRef type, std::vector<ObjectRef> v) : Object(type), value(v) {
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

TypeRef Thunk::type = create<Type>();

Thunk::Thunk(TypeRef type) : Object(type) {
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
