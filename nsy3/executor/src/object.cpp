#include "object.hpp"
#include "functionutils.hpp"
#include "exception.hpp"

#include <stdexcept>
#include <sstream>
#include <iostream>
#include "executionengine.hpp"

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

std::string Object::to_str() const {
    return type_->name() + "(?)";
}

TypeRef Object::obj_type() const {
    return type_;
}

ObjectRef Object::getattr(std::string name) const {
    auto obj = type_->get(name);
    if (dynamic_cast<const BuiltinFunction*>(obj.get())) {
        return create<BoundMethod>(shared_from_this(), obj);
    }
    if (dynamic_cast<const Property*>(obj.get())) {
        return obj->call({shared_from_this()});
    }
    return obj;
}

ObjectRef Object::call(const std::vector<ObjectRef>& /*args*/) const {
    throw std::runtime_error("Cannot call me");
}

std::size_t Object::hash() const {
    create<TypeException>("Type '" + type_->name() + "' is not hashable")->raise();
    return 0;
}

bool Object::eq(ObjectRef other) const {
    return false;
}

bool Object::to_bool() const {
    return true;
}

TypeRef Type::make_type_type() {
    auto obj = std::make_shared<Type>(nullptr, "Type");
    obj->type_ = obj;
    return obj;
}

TypeRef Type::type = Type::make_type_type();

Type::Type(TypeRef type, std::string name, std::map<std::string, ObjectRef> attrs) : Object(type), name_(name), attrs(attrs) {
}

std::string Type::to_str() const {
    return "Type(" + name_ + ")";
}

ObjectRef Type::get(std::string name) const {
    auto iter = attrs.find(name);
    if (iter == attrs.end()) {
        throw std::runtime_error("No such attr");
    }
    return iter->second;
}

ObjectRef Type::call(const std::vector<ObjectRef>& args) const {
    return get("__new__")->call(args);
}

TypeRef BuiltinFunction::type = create<Type>("BuiltinFunction");

TypeRef Integer::type = create<Type>("Integer", Type::attrmap{
    {"+", create<BuiltinFunction>([](int a, int b) { return a + b; })},
    {"-", create<BuiltinFunction>([](int a, int b) { return a - b; })},
    {"*", create<BuiltinFunction>([](int a, int b) { return a * b; })},
    {"/", create<BuiltinFunction>([](int a, int b) { return static_cast<double>(a) / b; })},
    {"//", create<BuiltinFunction>([](int a, int b) { return a / b; })},
    {"%", create<BuiltinFunction>([](int a, int b) { auto x = (a % b); return x > 0 ? x : x + b; })},
    {"==", create<BuiltinFunction>([](int a, ObjectRef b) {
        if (auto obj = dynamic_cast<const Integer*>(b.get())) return a == obj->get();
        return false;
    })},
    {"<", create<BuiltinFunction>([](int a, int b) { return a < b; })},
});

Integer::Integer(TypeRef type, int v) : Numeric(type), value(v) {
}

std::string Integer::to_str() const {
    return std::to_string(value);
}

bool Integer::to_bool() const {
    return value;
}

TypeRef Boolean::type = create<Type>("Boolean");

Boolean::Boolean(TypeRef type, bool v) : Integer(type, v) {
}

std::string Boolean::to_str() const {
    return value ? "TRUE" : "FALSE";
}

TypeRef Float::type = create<Type>("Float", Type::attrmap{
    {"+", create<BuiltinFunction>([](double a, double b){ return a + b; })},
    {"-", create<BuiltinFunction>([](double a, double b){ return a - b; })},
    {"*", create<BuiltinFunction>([](double a, double b){ return a * b; })},
    {"/", create<BuiltinFunction>([](double a, double b){ return static_cast<double>(a) / b; })},
    {"//", create<BuiltinFunction>([](double a, double b){ return static_cast<int>(a / b); })},
//     {"%", create<BuiltinFunction>([](double a, double b){ auto x = (a % b); return x > 0 ? x : x + b; })}
});

Float::Float(TypeRef type, double v) : Numeric(type), value(v) {
}

std::string Float::to_str() const {
    return std::to_string(value);
}

bool Float::to_bool() const {
    return value;
}

TypeRef String::type = create<Type>("String", Type::attrmap{
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

std::string String::to_str() const {
    return value;
}
TypeRef Bytes::type = create<Type>("Bytes", Type::attrmap{
    {"+", create<BuiltinFunction>([](std::string a, std::string b) { return a + b; })},
    {"*", create<BuiltinFunction>([](std::string a, int b) {
        std::string res(a.size() * std::max(0, b), '\0');
        for (auto i = 0; i < b; ++i) {
            res.replace(i * a.size(), a.size(), a);
        }
        return res;
    })}
});

std::size_t String::hash() const {
    return std::hash<std::string>{}(value);
}

bool String::eq(ObjectRef other) const {
    if (auto c_other = dynamic_cast<const String*>(other.get())) {
        return value == c_other->value;
    }
    return false;
}

Bytes::Bytes(TypeRef type, std::basic_string<unsigned char> v) : Object(type), value(v) {
}

std::string Bytes::to_str() const {
    return "Bytes";
}

TypeRef BoundMethod::type = create<Type>("BoundMethod");

BoundMethod::BoundMethod(TypeRef type, ObjectRef self, ObjectRef func) : Object(type), self(self), func(func) {
}

std::string BoundMethod::to_str() const {
    return "BoundMethod(" + self->to_str() + ", " + func->to_str() + ")";
}

ObjectRef BoundMethod::call(const std::vector<ObjectRef>& args) const {
    std::vector<ObjectRef> mod_args = args;
    mod_args.insert(mod_args.begin(), self);
    return func->call(mod_args);
}

TypeRef Property::type = create<Type>("Property");

Property::Property(TypeRef type, ObjectRef func) : Object(type), func(func) {
}

std::string Property::to_str() const {
    return "Property(" + func->to_str() + ")";
}

ObjectRef Property::call(const std::vector<ObjectRef>& args) const {
    return func->call(args);
}

TypeRef Dict::type = create<Type>("Dict");

Dict::Dict(TypeRef type, ObjectRefMap v) : Object(type), value(v) {
}

std::string Dict::to_str() const {
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

TypeRef List::type = create<Type>("List");

List::List(TypeRef type, std::vector<ObjectRef> v) : Object(type), value(v) {
}

std::string List::to_str() const {
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

TypeRef Thunk::type = create<Type>("Thunk");

Thunk::Thunk(TypeRef type, ExecutionEngine* execengine) : Object(type), execengine(execengine) {
}

Thunk::~Thunk() {
    if (!finalized) {
        std::cerr << "Thunk " << this << " was not finalized!" << std::endl;
    }
}

void Thunk::subscribe(std::shared_ptr<const Thunk> thunk) const {
    execengine->subscribe_thunk(std::dynamic_pointer_cast<const Thunk>(shared_from_this()), thunk);
}

void Thunk::notify(ObjectRef /*obj*/) const {
}

void Thunk::finalize(ObjectRef obj) const {
    const_cast<Thunk*>(this)->finalized = true;
    execengine->finalize_thunk(std::dynamic_pointer_cast<const Thunk>(shared_from_this()), std::move(obj));
}
