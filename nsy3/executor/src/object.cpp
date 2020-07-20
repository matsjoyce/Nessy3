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

ObjectRef no_thunks(const BaseObjectRef& r) {
    auto obj = std::dynamic_pointer_cast<const Object>(r);
    if (!r) {
        throw std::runtime_error("Thunks are not allowed to be returned for this method!");
    }
    return obj;
}

Object::Object(TypeRef type) : type_(type) {
    if (!type) {
        std::cerr << "NULL type!" << std::endl;
    }
}

std::string Object::to_str() const {
    return type_->name() + "(?)";
}

TypeRef Object::obj_type() const {
    return type_;
}

BaseObjectRef Object::getattr(std::string name) const {
    auto obj = gettype(name);
    if (dynamic_cast<const Property*>(obj.get())) {
        return obj->call({self()});
    }
    return obj;
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
        return create<BoundMethod>(self(), obj);
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
        return create<BoundMethod>(self(), obj);
    }
    return obj;
}

BaseObjectRef Object::call(const std::vector<ObjectRef>& /*args*/) const {
    throw std::runtime_error("Cannot call me");
}

ObjectRef Object::call_no_thunks(const std::vector<ObjectRef>& args) const {
    return no_thunks(call(args));
}


std::size_t Object::hash() const {
    create<TypeException>("Type '" + type_->name() + "' is not hashable")->raise();
    return 0;
}

bool Object::eq(ObjectRef other) const {
    try {
        return no_thunks(gettype("==")->call({other}))->to_bool();
    }
    catch (const ObjectRef& exc) {
        if (dynamic_cast<const UnsupportedOperation*>(exc.get())) {
            return std::dynamic_pointer_cast<const Object>(other->gettype("r==")->call({self()}))->to_bool();
        }
        throw;
    }
}

bool Object::to_bool() const {
    return true;
}

std::vector<TypeRef> make_top_types() {
    auto type_type = std::make_shared<Type>(nullptr, "Type", Type::basevec{});
    type_type->type_ = type_type;
    Type::type = type_type;
    auto bf_type = std::make_shared<Type>(type_type, "BuiltinFunction", Type::basevec{});
    BuiltinFunction::type = bf_type;
    auto p_type = std::make_shared<Type>(type_type, "Property", Type::basevec{});
    Property::type = p_type;

    auto unsupported_op = [](std::string op) {
        return create<BuiltinFunction>([op](ObjectRef a, ObjectRef b) -> ObjectRef {
            create<UnsupportedOperation>(
                "Objects of types '" + a->obj_type()->name()
                + "' and '" + b->obj_type()->name() + "' do not support the operator '" + op + "'"
            )->raise();
            return {};
        });
    };

    Type::attrmap obj_attrs = {
        {"<=>", unsupported_op("<=>")},
        {"==", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            try {
                return convert<int>(a->gettype("<=>")->call_no_thunks({b})) == 0;
            }
            catch (const ObjectRef& exc) {
                if (dynamic_cast<const UnsupportedOperation*>(exc.get())) {
                    return a.get() == b.get();
                }
                throw;
            }
        })},
        {"r==", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("==")->call({b});
        })},
        {"!=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return !convert<bool>(a->gettype("==")->call_no_thunks({b}));
        })},
        {"r!=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("!=")->call({b});
        })},
        {"<", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call_no_thunks({b})) == -1;
        })},
        {"r<", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype(">=")->call_no_thunks({b}));
        })},
        {">", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call_no_thunks({b})) == 1;
        })},
        {"r>", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype("<=")->call_no_thunks({b}));
        })},
        {"<=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call_no_thunks({b})) != 1;
        })},
        {"r<=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype(">")->call_no_thunks({b}));
        })},
        {">=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<int>(a->gettype("<=>")->call_no_thunks({b})) != -1;
        })},
        {"r>=", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return convert<bool>(a->gettype("<")->call_no_thunks({b}));
        })},
        {"+", unsupported_op("+")},
        {"r+", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("+")->call({b});
        })},
        {"-", unsupported_op("-")},
        {"r-", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("-")->call_no_thunks({b})->gettype("u-")->call({});
        })},
        {"*", unsupported_op("*")},
        {"r*", create<BuiltinFunction>([](ObjectRef a, ObjectRef b) {
            return a->gettype("*")->call({b});
        })},
        {"/", unsupported_op("/")},
        {"r/", unsupported_op("r/")},
        {"//", unsupported_op("//")},
        {"r//", unsupported_op("r//")},
        {"%", unsupported_op("%")},
        {"r%", unsupported_op("r%")},
        {"**", unsupported_op("**")},
        {"r**", unsupported_op("r**")},
        {"[]", unsupported_op("[]")},
        {"__type__", create<Property>(create<BuiltinFunction>(method(&Object::obj_type)))}
    };

    auto obj_type = std::make_shared<Type>(type_type, "Object", Type::basevec{}, obj_attrs);
    Object::type = obj_type;
    type_type->bases_ = type_type->mro_ = bf_type->bases_ = bf_type->mro_ = p_type->bases_ = p_type->mro_ = {obj_type};
    return {type_type, obj_type, bf_type};
}

// Defining the top of the hierarchy is ... difficult. And highly recursive.
TypeRef Type::type;
TypeRef Object::type;
TypeRef BuiltinFunction::type;
TypeRef Property::type;
static auto top_types = make_top_types();

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

BaseObjectRef Type::getattr(std::string name) const {
    auto iter = attrs_.find(name);
    if (iter == attrs_.end()) {
        return Object::getattr(name);
    }
    return iter->second;
}

BaseObjectRef Type::call(const std::vector<ObjectRef>& args) const {
    return no_thunks(getattr("__new__"))->call(args);
}

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
    {"+", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value + other_int->value);
        }
        return self->getsuper(Integer::type, "+")->call({other});
    })},
    {"-", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value - other_int->value);
        }
        return self->getsuper(Integer::type, "-")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value * other_int->value);
        }
        return self->getsuper(Integer::type, "*")->call({other});
    })},
    {"/", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Float>(static_cast<double>(self->value) / other_int->value);
        }
        return self->getsuper(Integer::type, "/")->call({other});
    })},
    {"//", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value / other_int->value);
        }
        return self->getsuper(Integer::type, "//")->call({other});
    })},
    {"%", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(self->value % other_int->value + (self->value < 0 ? other_int->value : 0));
        }
        return self->getsuper(Integer::type, "%")->call({other});
    })},
    {"**", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_int = dynamic_cast<const Integer*>(other.get())) {
            return create<Integer>(intpow(self->value, other_int->value));
        }
        return self->getsuper(Integer::type, "**")->call({other});
    })},
    {"<=>", create<BuiltinFunction>([](const Integer* self, ObjectRef other) -> BaseObjectRef {
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
// TODO long winded since I might make the constructor private
std::shared_ptr<const Boolean> Boolean::true_ = std::shared_ptr<Boolean>(new Boolean(Boolean::type, 1));
std::shared_ptr<const Boolean> Boolean::false_ = std::shared_ptr<Boolean>(new Boolean(Boolean::type, 0));

Boolean::Boolean(TypeRef type, bool v) : Integer(type, v) {
}

std::string Boolean::to_str() const {
    return value ? "TRUE" : "FALSE";
}

TypeRef Float::type = create<Type>("Float", Type::basevec{Numeric::type}, Type::attrmap{
    {"u-", create<BuiltinFunction>([](const Float* self) -> ObjectRef {
        return create<Float>(-self->value);
    })},
    {"+", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value + other_num->to_double());
        }
        return self->getsuper(Float::type, "+")->call({other});
    })},
    {"-", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value - other_num->to_double());
        }
        return self->getsuper(Float::type, "-")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value * other_num->to_double());
        }
        return self->getsuper(Float::type, "*")->call({other});
    })},
    {"/", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(self->value / other_num->to_double());
        }
        return self->getsuper(Float::type, "/")->call({other});
    })},
    {"r/", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(other_num->to_double() / self->value);
        }
        return self->getsuper(Float::type, "r/")->call({other});
    })},
    {"//", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Integer>(self->value / other_num->to_double());
        }
        return self->getsuper(Float::type, "//")->call({other});
    })},
    {"r//", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Integer>(other_num->to_double() / self->value);
        }
        return self->getsuper(Float::type, "r//")->call({other});
    })},
    {"%", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Float>(std::fmod(self->value, other_num->to_double()));
        }
        return self->getsuper(Float::type, "%")->call({other});
    })},
    {"r%", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            // TODO Large numbers may botch this up
            return create<Float>(std::fmod(other_num->to_double(), self->value));
        }
        return self->getsuper(Float::type, "r%")->call({other});
    })},
    {"**", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(std::pow(self->value, other_num->to_double()));
        }
        return self->getsuper(Float::type, "**")->call({other});
    })},
    {"r**", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_num = dynamic_cast<const Numeric*>(other.get())) {
            return create<Float>(std::pow(other_num->to_double(), self->value));
        }
        return self->getsuper(Float::type, "r**")->call({other});
    })},
    {"<=>", create<BuiltinFunction>([](const Float* self, ObjectRef other) -> BaseObjectRef {
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
    {"+", create<BuiltinFunction>([](const String* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_s = dynamic_cast<const String*>(other.get())) {
            return create<String>(self->value + other_s->value);
        }
        return self->getsuper(String::type, "+")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const String* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_i = dynamic_cast<const Integer*>(other.get())) {
            std::string res(self->value.size() * std::max(0l, other_i->get()), '\0');
            for (auto i = other_i->get(); i > 0;) {
                res.replace(--i * self->value.size(), self->value.size(), self->value);
            }
            return create<String>(res);
        }
        return self->getsuper(String::type, "*")->call({other});
    })},
    {"==", create<BuiltinFunction>([](const String* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_s = dynamic_cast<const String*>(other.get())) {
            return create<Boolean>(self->get() == other_s->get());
        }
        return self->getsuper(String::type, "==")->call({other});
    })},
});

String::String(TypeRef type, std::string v) : Object(type), value(v) {
}

std::string String::to_str() const {
    return value;
}

std::size_t String::hash() const {
    return std::hash<std::string>{}(value);
}

TypeRef Bytes::type = create<Type>("Bytes", Type::basevec{Object::type}, Type::attrmap{
    {"+", create<BuiltinFunction>([](const Bytes* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_s = dynamic_cast<const Bytes*>(other.get())) {
            return create<Bytes>(self->value + other_s->value);
        }
        return self->getsuper(Bytes::type, "+")->call({other});
    })},
    {"*", create<BuiltinFunction>([](const Bytes* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_i = dynamic_cast<const Integer*>(other.get())) {
            std::basic_string<unsigned char> res(self->value.size() * std::max(0l, other_i->get()), '\0');
            for (auto i = other_i->get(); i;) {
                res.replace(--i * self->value.size(), self->value.size(), self->value);
            }
            return create<Bytes>(res);
        }
        return self->getsuper(Bytes::type, "*")->call({other});
    })},
    {"==", create<BuiltinFunction>([](const Bytes* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_s = dynamic_cast<const Bytes*>(other.get())) {
            return create<Boolean>(self->get() == other_s->get());
        }
        return self->getsuper(Bytes::type, "==")->call({other});
    })},
});

Bytes::Bytes(TypeRef type, std::basic_string<unsigned char> v) : Object(type), value(v) {
}

std::string Bytes::to_str() const {
    return "Bytes";
}

TypeRef NoneType::type = create<Type>("NoneType", Type::basevec{Object::type});
std::shared_ptr<const NoneType> NoneType::none = std::shared_ptr<NoneType>(new NoneType(NoneType::type));

NoneType::NoneType(TypeRef type) : Object(type) {
}

std::string NoneType::to_str() const {
    return "NONE";
}

bool NoneType::to_bool() const {
    return false;
}


TypeRef BoundMethod::type = create<Type>("BoundMethod", Type::basevec{Object::type});

BoundMethod::BoundMethod(TypeRef type, ObjectRef self, ObjectRef func) : Object(type), self(self), func(func) {
}

std::string BoundMethod::to_str() const {
    return "BoundMethod(" + self->to_str() + ", " + func->to_str() + ")";
}

BaseObjectRef BoundMethod::call(const std::vector<ObjectRef>& args) const {
    std::vector<ObjectRef> mod_args = args;
    mod_args.insert(mod_args.begin(), self);
    return func->call(mod_args);
}

Property::Property(TypeRef type, ObjectRef func) : Object(type), func(func) {
}

std::string Property::to_str() const {
    return "Property(" + func->to_str() + ")";
}

BaseObjectRef Property::call(const std::vector<ObjectRef>& args) const {
    return func->call(args);
}

TypeRef Dict::type = create<Type>("Dict", Type::basevec{Object::type}, Type::attrmap{
    {"[]", create<BuiltinFunction>([](const Dict* self, ObjectRef key) {
        auto iter = self->value.find(key);
        if (iter == self->value.end()) {
            create<IndexException>("No such key")->raise();
        }
        return iter->second;
    })}
});

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

TypeRef List::type = create<Type>("List", Type::basevec{Object::type}, Type::attrmap{
    {"[]", create<BuiltinFunction>([](const List* self, const Integer* idx) {
        if (idx->get() < 0 || idx->get() >= static_cast<int64_t>(self->value.size())) {
            create<IndexException>("Index is out of bounds")->raise();
        }
        return self->value[idx->get()];
    })},
    {"__iter__", create<BuiltinFunction>([](std::shared_ptr<const List> self) -> ObjectRef {
        return create<ListIterator>(self, 0);
    })},
    {"==", create<BuiltinFunction>([](const List* self, ObjectRef other) -> BaseObjectRef {
        if (auto other_l = dynamic_cast<const List*>(other.get())) {
            if (self->value.size() != other_l->value.size()) {
                return Boolean::false_;
            }
            for (auto iter_a = self->value.begin(), iter_b = other_l->value.begin(); iter_a != self->value.end(); ++iter_a, ++iter_b) {
                if (!(*iter_a)->eq(*iter_b)) {
                    return Boolean::false_;
                }
            }
            return Boolean::true_;
        }
        return self->getsuper(List::type, "==")->call({other});
    })},
    {":+", create<BuiltinFunction>([](const List* self, ObjectRef obj) {
        std::vector<ObjectRef> res = self->value;
        res.push_back(obj);
        return create<List>(res);
    })},
});

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

TypeRef ListIterator::type = create<Type>("ListIterator", Type::basevec{Object::type}, Type::attrmap{
    {"__iter__", create<BuiltinFunction>([](ObjectRef self) -> ObjectRef {
        return self;
    })},
    {"__next__", create<BuiltinFunction>([](const ListIterator* self) -> ObjectRef {
        if (self->position >= self->list->get().size() ) {
            return NoneType::none;
        }
        std::vector<ObjectRef> res = {create<ListIterator>(self->list, self->position + 1), self->list->get()[self->position]};
        return create<List>(res);
    })}
});

ListIterator::ListIterator(TypeRef type, std::shared_ptr<const List> list, unsigned int position) : Object(type), list(list), position(position) {
}
