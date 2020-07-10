#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>

class Object;
using ObjectRef = std::shared_ptr<const Object>;
class Type;
using TypeRef = std::shared_ptr<const Type>;
// List of objects that have been created (in the early phases of initialisation) without types

class Object : public std::enable_shared_from_this<Object> {
    TypeRef type_;
public:
    Object(TypeRef type);
    virtual ~Object() = default;
    virtual std::string to_str() const;
    virtual bool to_bool() const;
    virtual std::size_t hash() const;
    virtual bool eq(ObjectRef other) const;
    TypeRef obj_type() const;
    ObjectRef getattr(std::string name) const;
    virtual ObjectRef call(const std::vector<ObjectRef>& args) const;

    friend class Type;
};

template<class T, class... Args> std::shared_ptr<const T> create(Args... args) {
    return std::make_shared<T>(T::type, args...);
}

std::ostream& operator<<(std::ostream& s, const ObjectRef& obj);

class Type : public Object {
    std::string name_;
    std::map<std::string, ObjectRef> attrs;
    static TypeRef make_type_type();
public:
    Type(TypeRef type, std::string name, std::map<std::string, ObjectRef> attr={});
    std::string to_str() const override;
    static TypeRef type;
    ObjectRef get(std::string name)const ;
    ObjectRef call(const std::vector<ObjectRef>& args) const override;
    std::string name() const { return name_; }

    using attrmap = std::map<std::string, ObjectRef>;
};

struct AbstractFunctionHolder {
    virtual ObjectRef call(const std::vector<ObjectRef>& args) const = 0;
    virtual ~AbstractFunctionHolder() = default;
};

template<class T> struct convert_from_objref {
    static T convert(const ObjectRef& objref);
};
template<class T> struct convert_to_objref {
    static ObjectRef convert(const T& t);
};

template<class T, class... Args> struct FunctionHolder : public AbstractFunctionHolder {
    std::function<T(Args...)> f;
    FunctionHolder(std::function<T(Args...)> f) : f(f) {}
    ObjectRef call(const std::vector<ObjectRef>& args) const {
        return call(args, std::index_sequence_for<Args...>{});
    }
    template<class I, I... Ints> ObjectRef call(const std::vector<ObjectRef>& args, std::integer_sequence<I, Ints...>) const {
        if (sizeof...(Args) != args.size()) {
            throw std::runtime_error("Wrong number of args");
        }
        return convert_to_objref<T>::convert(f(convert_from_objref<Args>::convert(args[Ints])...));
    }
};

template<class T> struct FunctionHolder<T, const std::vector<ObjectRef>&> : public AbstractFunctionHolder {
    std::function<T(const std::vector<ObjectRef>&)> f;
    FunctionHolder(std::function<T(const std::vector<ObjectRef>&)> f) : f(f) {}
    ObjectRef call(const std::vector<ObjectRef>& args) const { return convert_to_objref<T>::convert(f(args)); }
};

class BuiltinFunction : public Object {
    std::unique_ptr<AbstractFunctionHolder> func;
public:
    template<class F> BuiltinFunction(TypeRef type, F f) : Object(type), func(new FunctionHolder(std::function(f))) {}
    static TypeRef type;
    ObjectRef call(const std::vector<ObjectRef>& args) const override { return func->call(args); }
};

class Numeric : public Object {
public:
    using Object::Object;
    static TypeRef type;
    virtual double to_double() const = 0;
};

class Integer : public Numeric {
protected:
    int value;
public:
    Integer(TypeRef type, int v);
    std::string to_str() const override;
    static TypeRef type;
    int get() const { return value; }
    double to_double() const override { return value; }
    bool to_bool() const override;
};

class Float : public Numeric {
    double value;
public:
    Float(TypeRef type, double v);
    std::string to_str() const override;
    static TypeRef type;
    double get() const { return value; }
    double to_double() const override { return value; }
    bool to_bool() const override;
};

class Boolean : public Integer {
public:
    Boolean(TypeRef type, bool v);
    std::string to_str() const override;
    static TypeRef type;
};

class String : public Object {
    std::string value;
public:
    String(TypeRef type, std::string v);
    std::string to_str() const override;
    static TypeRef type;
    std::string get() const { return value; }
    std::size_t hash() const override;
    bool eq(ObjectRef other) const override;
};

class Bytes : public Object {
    std::basic_string<unsigned char> value;
public:
    Bytes(TypeRef type, std::basic_string<unsigned char> v);
    std::string to_str() const override;
    static TypeRef type;
    std::basic_string<unsigned char> get() const { return value; }
};

class BoundMethod : public Object {
    ObjectRef self, func;
public:
    BoundMethod(TypeRef type, ObjectRef self, ObjectRef func);
    std::string to_str() const override;
    static TypeRef type;
    ObjectRef call(const std::vector<ObjectRef>& args) const override;
};

class Property : public Object {
    ObjectRef func;
public:
    Property(TypeRef type, ObjectRef func);
    std::string to_str() const override;
    static TypeRef type;
    ObjectRef call(const std::vector<ObjectRef>& args) const override;
};

struct ObjectRefHash {
    inline std::size_t operator()(const ObjectRef& obj) const {
        return obj->hash();
    }
};

struct ObjectRefEq {
    inline bool operator()(const ObjectRef& lhs, const ObjectRef& rhs) const {
        return lhs->eq(rhs);
    }
};

using ObjectRefMap = std::unordered_map<ObjectRef, ObjectRef, ObjectRefHash, ObjectRefEq>;

class Dict : public Object {
    ObjectRefMap value;
public:
    Dict(TypeRef type, ObjectRefMap v);
    std::string to_str() const override;
    static TypeRef type;
    const ObjectRefMap& get() const { return value; }
};

class List : public Object {
    std::vector<ObjectRef> value;
public:
    List(TypeRef type, std::vector<ObjectRef> v);
    std::string to_str() const override;
    static TypeRef type;
    const std::vector<ObjectRef>& get() const { return value; }
};

class ExecutionEngine;

class Thunk : public Object {
    ExecutionEngine* execengine;
    bool finalized = false;
public:
    Thunk(TypeRef type, ExecutionEngine* execengine);
    ~Thunk();
    static TypeRef type;
    // Naughty method
    void subscribe(std::shared_ptr<const Thunk> thunk) const;
    virtual void notify(ObjectRef obj) const;
    void finalize(ObjectRef obj) const;
    ExecutionEngine* execution_engine() const { return execengine; }
};

#endif // OBJECT_HPP
