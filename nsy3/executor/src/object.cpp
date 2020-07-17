#include "object.hpp"
#include "functionutils.hpp"
#include "exception.hpp"

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cmath>
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
    return gettype(name);
}

ObjectRef Object::gettype(std::string name) const {
    ObjectRef obj;
    {
        auto iter = type_->attrs().find(name);
        if (iter != type_->attrs().end()) {
            obj = iter->second;
        }
        else {
            auto& mro = type_->mro();
            for (auto iter = mro.begin(); iter != mro.end(); ++iter) {
                auto titer = (*iter)->attrs().find(name);
                if (titer != (*iter)->attrs().end()) {
                    obj = titer->second;
                    break;
                }
            }
        }
    }
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

ObjectRef Object::getsuper(TypeRef type, std::string name) const {
    auto& mro = type_->mro();
    ObjectRef obj;

    for (auto iter = type == type_ ? mro.begin() : std::find(mro.begin(), mro.end(), type); iter != mro.end(); ++iter) {
        auto titer = (*iter)->attrs().find(name);
        if (titer != (*iter)->attrs().end()) {
            obj = titer->second;
            break;
        }
    }
    if (!obj) {
        create<NameException>("Super object of type '" + type_->name() + "' using super of '" + type->name() + "' has no attribute '" + name + "'")->raise();
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
            create<UnsupportedOperation>(
                "Objects of types '" + b->obj_type()->name()
                + "' and '" + a->obj_type()->name() + "' do not support the operator '" + op + "'"
            )->raise();
            return {};
        });
    };

    Type::attrmap obj_attrs = {
        {"<=>", unsupported_op("<=>")},
        {"==", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            try {
                std::cerr << "A==" << std::endl;
                return convert<int>(a->gettype("<=>")->call({b})) == 0;
            }
            catch (const ObjectRef& exc) {
                if (dynamic_cast<const UnsupportedOperation*>(exc.get())) {
                    std::cerr << "B==" << std::endl;
                    return a.get() == b.get();
                }
                throw;
            }
        })},
        {"r==", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("==")->call({b});
        })},
        {"!=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return !convert<bool>(a->gettype("==")->call({b}));
        })},
        {"r!=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("!=")->call({b});
        })},
        {"<", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call({b})) == -1;
        })},
        {"r<", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype(">=")->call({b}));
        })},
        {">", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call({b})) == 1;
        })},
        {"r>", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype("<=")->call({b}));
        })},
        {"<=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call({b})) != 1;
        })},
        {"r<=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return convert<bool>(a->gettype(">")->call({b})); })},
        {">=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call({b})) != -1;
        })},
        {"r>=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return convert<bool>(a->gettype("<")->call({b})); })},
        {"+", unsupported_op("+")},
        {"r+", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return a->gettype("+")->call({b}); })},
        {"-", unsupported_op("-")},
        {"r-", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return a->gettype("-")->call({b})->gettype("u-")->call({}); })},
        {"*", unsupported_op("*")},
        {"r*", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) { return a->gettype("*")->call({b}); })},
        {"/", unsupported_op("/")},
        {"r/", unsupported_op("r/")},
        {"//", unsupported_op("//")},
        {"r//", unsupported_op("r//")},
        {"%", unsupported_op("%")},
        {"r%", unsupported_op("r%")},
        {"**", unsupported_op("**")},
        {"r**", unsupported_op("r**")},
        {"__type__", create<Property>(create<BuiltinFunction>(method(&Object::obj_type)))}
    };

    auto obj_type = std::make_shared<Type>(nullptr, "Object", Type::basevec{}, obj_attrs);
    type_type->bases_ = type_type->mro_ = {obj_type};
    return {type_type, obj_type};
}

static auto top_types = make_top_types();
TypeRef Type::type = top_types.first;
TypeRef Object::type = top_types.second;

std::vector<TypeRef> Type::make_mro(const std::vector<TypeRef>& bases) {
    // C3 superclass linearization algorithm
    std::vector<TypeRef> mro;
    std::vector<std::vector<TypeRef>> base_bases;
    for (auto& base : bases) {
        std::vector<TypeRef> b = {base};
        b.insert(b.end(), base->mro_.begin(), base->mro_.end());
        base_bases.push_back(b);
    }
    while (base_bases.size()) {
        TypeRef candidate;
        for (auto& group : base_bases) {
            bool good = true;
            for (auto& inner_group : base_bases) {
                if (std::find(++inner_group.begin(), inner_group.end(), group.front()) != inner_group.end()) {
                    good = false;
                    break;
                }
            }
            if (good) {
                candidate = group.front();
                break;
            }
        }
        if (!candidate) {
            throw std::runtime_error("Cannot resolve MRO!");
        }
        mro.push_back(candidate);
        for (auto iter = base_bases.begin(); iter != base_bases.end();) {
            if (iter->front() == candidate) {
                iter->erase(iter->begin());
                if (!iter->size()) {
                    iter = base_bases.erase(iter);
                    continue;
                }
            }
            ++iter;
        }
    }
    return mro;
}

Type::Type(TypeRef type, std::string name, std::vector<TypeRef> bases, std::map<std::string, ObjectRef> attrs)
    : Object(type), name_(name), bases_(bases), mro_(make_mro(bases)), attrs_(attrs) {
    std::cout << "MRO for " << name << std::endl;
    for (auto m : mro_) {
        std::cout << "  " << m->name() << std::endl;
    }
}

std::string Type::to_str() const {
    return "Type(" + name_ + ")";
}

ObjectRef Type::getattr(std::string name) const {
    auto iter = attrs_.find(name);
    if (iter == attrs_.end()) {
        return Object::getattr(name);
    }
    return iter->second;
}

ObjectRef Type::call(const std::vector<ObjectRef>& args) const {
    return getattr("__new__")->call(args);
}

TypeRef BuiltinFunction::type = create<Type>("BuiltinFunction", Type::basevec{Object::type});

TypeRef Numeric::type = create<Type>("Numeric", Type::basevec{Object::type}, Type::attrmap{
});

int64_t intpow(int64_t x, int64_t n) {
    int64_t r = 1;
    while (n) {
        if (n & 1) {
            r *= x;
            --n;
        }
        else {
            x *= x;
            n >>= 1;
        }
    }
    return r;
}

TypeRef Integer::type = create<Type>("Integer", Type::basevec{Numeric::type}, Type::attrmap{
    {"u-", create<BuiltinFunction>([](const Integer* self) -> ObjectRef {
        return create<Integer>(-self->value);
    })},
    {"+", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value + other_int->value);
        }
        return self->getsuper(Integer::type, "+")->call({other});
    })},
    {"-", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value - other_int->value);
        }
        return self->getsuper(Integer::type, "-")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value * other_int->value);
        }
        return self->getsuper(Integer::type, "*")->call({other});
    })},
    {"/", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Float>(static_cast<double>(self->value) / other_int->value);
        }
        return self->getsuper(Integer::type, "/")->call({other});
    })},
    {"//", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value / other_int->value);
        }
        return self->getsuper(Integer::type, "//")->call({other});
    })},
    {"%", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value % other_int->value + (self->value < 0 ? other_int->value : 0));
        }
        return self->getsuper(Integer::type, "%")->call({other});
    })},
    {"**", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(intpow(self->value, other_int->value));
        }
        return self->getsuper(Integer::type, "**")->call({other});
    })},
    {"<=>", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> ObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value == other_int->value ? 0 : (self->value < other_int->value ? -1 : 1));
        }
        return self->getsuper(Integer::type, "<=>")->call({other});
    })},
});

Integer::Integer(TypeRef type, int64_t v) : Numeric(type), value(v) {
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
    {"u-", create<BuiltinFunction>([](const Float* self) -> ObjectRef {
        return create<Float>(-self->value);
    })},
    {"+", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value + other_num->to_double());
        }
        return self->getsuper(Float::type, "+")->call({other});
    })},
    {"-", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value - other_num->to_double());
        }
        return self->getsuper(Float::type, "-")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value * other_num->to_double());
        }
        return self->getsuper(Float::type, "*")->call({other});
    })},
    {"/", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value / other_num->to_double());
        }
        return self->getsuper(Float::type, "/")->call({other});
    })},
    {"r/", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(other_num->to_double() / self->value);
        }
        return self->getsuper(Float::type, "r/")->call({other});
    })},
    {"//", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Integer>(self->value / other_num->to_double());
        }
        return self->getsuper(Float::type, "//")->call({other});
    })},
    {"r//", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Integer>(other_num->to_double() / self->value);
        }
        return self->getsuper(Float::type, "r//")->call({other});
    })},
    {"%", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Float>(std::fmod(self->value, other_num->to_double()));
        }
        return self->getsuper(Float::type, "%")->call({other});
    })},
    {"r%", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Float>(std::fmod(other_num->to_double(), self->value));
        }
        return self->getsuper(Float::type, "r%")->call({other});
    })},
    {"**", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(std::pow(self->value, other_num->to_double()));
        }
        return self->getsuper(Float::type, "**")->call({other});
    })},
    {"r**", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(std::pow(other_num->to_double(), self->value));
        }
        return self->getsuper(Float::type, "r**")->call({other});
    })},
    {"<=>", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> ObjectRef {
        if (auto other_float = dynamic_cast<const Float*>(other.get())) {
            return create<Integer>(self->value == other_float->value ? 0 : (self->value < other_float->value ? -1 : 1));
        }
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            // TODO: For large integers, this will fail.
            return create<Integer>(self->value == other_int->get() ? 0 : (self->value < other_int->get() ? -1 : 1));
        }
        return self->getsuper(Float::type, "<=>")->call({other});
    })},
    {"__new__", create<BuiltinFunction>([](const Numeric* n){ return create<Float>(n->to_double()); })}
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
