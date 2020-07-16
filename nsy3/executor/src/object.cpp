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
    auto obj = type_->getattr(name);
    if (!obj) {
        create<NameException>("Object of type '" + type_->name() + "' has no attribute '" + name + "'")->raise();
    }
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
    return other.get() == this;
}

bool Object::to_bool() const {
    return true;
}

std::pair<TypeRef, TypeRef> make_top_types() {
    auto type_type = std::make_shared<Type>(nullptr, "Type", Type::basevec{});
    type_type->type_ = type_type;

    auto unsupported_op = [](std::string op) {
        return create<BuiltinFunction>([op](ObjectRef a, ObjectRef b) -> ObjectRef {
            create<TypeException>(
                "Objects of types '" + b->obj_type()->name()
                + "' and '" + a->obj_type()->name() + "' do not support the operator '" + op + "'"
            )->raise();
            return {};
        });
    };

    Type::attrmap obj_attrs = {
        {"==", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return a.get() == b.get(); })},
        {"!=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return !convert<bool>(a->getattr("==")->call({b}));
        })},
        {"<", unsupported_op("<")},
        {">", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return !convert<bool>(a->getattr("==")->call({b})) && !convert<bool>(a->getattr("<")->call({b}));
        })},
        {"<=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->getattr("==")->call({b})) || convert<bool>(a->getattr("<")->call({b}));
        })},
        {">=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return !convert<bool>(a->getattr("<")->call({b}));
        })},
    };

    auto add_ref_op = [&](std::string op) {
        auto rop = "r" + op;
        obj_attrs[op] = create<BuiltinFunction>([rop](ObjectRef a, ObjectRef b) { return b->getattr(rop)->call({a}); });
        obj_attrs[rop] = unsupported_op(op);
    };

    add_ref_op("+");
    add_ref_op("-");
    add_ref_op("*");
    add_ref_op("/");
    add_ref_op("//");
    add_ref_op("%");
    add_ref_op("**");

    auto obj_type = std::make_shared<Type>(nullptr, "Type", Type::basevec{}, obj_attrs);
    type_type->bases_ = {obj_type};
    return {type_type, obj_type};
}

static auto top_types = make_top_types();
TypeRef Type::type = top_types.first;
TypeRef Object::type = top_types.second;

Type::Type(TypeRef type, std::string name, std::vector<TypeRef> bases, std::map<std::string, ObjectRef> attrs) : Object(type), name_(name), bases_(bases), attrs(attrs) {
}

std::string Type::to_str() const {
    return "Type(" + name_ + ")";
}

ObjectRef Type::getattr(std::string name) const {
    auto iter = attrs.find(name);
    if (iter == attrs.end()) {
        for (auto& base : bases_) {
            if (auto obj = base->getattr(name)) {
                return obj;
            }
        }
        return {};
    }
    return iter->second;
}

ObjectRef Type::call(const std::vector<ObjectRef>& args) const {
    return getattr("__new__")->call(args);
}

TypeRef BuiltinFunction::type = create<Type>("BuiltinFunction", Type::basevec{Object::type});

TypeRef Numeric::type = create<Type>("Numeric", Type::basevec{Object::type}, Type::attrmap{
});

TypeRef Integer::type = create<Type>("Integer", Type::basevec{Numeric::type}, Type::attrmap{
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

TypeRef Boolean::type = create<Type>("Boolean", Type::basevec{Numeric::type});

Boolean::Boolean(TypeRef type, bool v) : Integer(type, v) {
}

std::string Boolean::to_str() const {
    return value ? "TRUE" : "FALSE";
}

TypeRef Float::type = create<Type>("Float", Type::basevec{Numeric::type}, Type::attrmap{
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

TypeRef String::type = create<Type>("String", Type::basevec{Object::type}, Type::attrmap{
    {"+", create<BuiltinFunction>([](std::string a, std::string b) { return a + b; })},
    {"*", create<BuiltinFunction>([](std::string a, int b) {
        std::string res(a.size() * std::max(0, b), '\0');
        for (auto i = 0; i < b; ++i) {
            res.replace(i * a.size(), a.size(), a);
        }
        return res;
    })},
    {"==", create<BuiltinFunction>([](std::string a, ObjectRef b) {
        if (auto obj = dynamic_cast<const String*>(b.get())) return a == obj->get();
        return false;
    })},
});

String::String(TypeRef type, std::string v) : Object(type), value(v) {
}

std::string String::to_str() const {
    return value;
}
TypeRef Bytes::type = create<Type>("Bytes", Type::basevec{Object::type}, Type::attrmap{
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

TypeRef BoundMethod::type = create<Type>("BoundMethod", Type::basevec{Object::type});

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

TypeRef Property::type = create<Type>("Property", Type::basevec{Object::type});

Property::Property(TypeRef type, ObjectRef func) : Object(type), func(func) {
}

std::string Property::to_str() const {
    return "Property(" + func->to_str() + ")";
}

ObjectRef Property::call(const std::vector<ObjectRef>& args) const {
    return func->call(args);
}

TypeRef Dict::type = create<Type>("Dict", Type::basevec{Object::type});

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

TypeRef List::type = create<Type>("List", Type::basevec{Object::type});

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

TypeRef Thunk::type = create<Type>("Thunk", Type::basevec{Object::type});

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

TypeRef Module::type = create<Type>("Module", Type::basevec{Object::type});

Module::Module(TypeRef type, std::string name, std::map<std::string, ObjectRef> v) : Object(type), name(name), value(v) {
}

std::string Module::to_str() const {
    return "Module(" + name + ")";
}

ObjectRef Module::getattr(std::string name) const {
    auto iter = value.find(name);
    if (iter == value.end()) {
        return Object::getattr(name);
    }
    return iter->second;
}
