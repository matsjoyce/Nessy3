#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>

class Object;
using ObjectRef = std::shared_ptr<Object>;
class Type;
using TypeRef = std::shared_ptr<Type>;
// List of objects that have been created (in the early phases of initialisation) without types
extern std::vector<std::pair<std::weak_ptr<Object>, TypeRef*>> untyped_objects;

class Object : public std::enable_shared_from_this<Object> {
    TypeRef type_;
public:
    Object(TypeRef type);
    virtual ~Object() = default;
    virtual std::string to_str();
    virtual bool to_bool();
//     virtual std::size_t hash();
    TypeRef obj_type();
    ObjectRef getattr(std::string name);
    virtual ObjectRef call(const std::vector<ObjectRef>& args);

    friend class Type;
};

template<class T, class... Args> std::shared_ptr<T> create(Args... args) {
    auto obj = std::make_shared<T>(T::type, args...);
    if (!T::type) {
        untyped_objects.push_back({obj, &T::type});
    }
    return obj;
}

std::ostream& operator<<(std::ostream& s, const ObjectRef& obj);

class Type : public Object {
    std::string name_;
    std::map<std::string, ObjectRef> attrs;
    static TypeRef make_type_type();
public:
    Type(TypeRef type, std::string name, std::map<std::string, ObjectRef> attr={});
    std::string to_str() override;
    static TypeRef type;
    ObjectRef get(std::string name);
    ObjectRef call(const std::vector<ObjectRef>& args) override;
    std::string name() const { return name_; }

    using attrmap = std::map<std::string, ObjectRef>;
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
    std::string to_str() override;
    static TypeRef type;
    int get() { return value; }
    double to_double() const override { return value; }
    bool to_bool() override;
};

class Float : public Numeric {
    double value;
public:
    Float(TypeRef type, double v);
    std::string to_str() override;
    static TypeRef type;
    double get() const { return value; }
    double to_double() const override { return value; }
    bool to_bool() override;
};

class Boolean : public Integer {
public:
    Boolean(TypeRef type, bool v);
    std::string to_str() override;
    static TypeRef type;
};

class String : public Object {
    std::string value;
public:
    String(TypeRef type, std::string v);
    std::string to_str() override;
    static TypeRef type;
    std::string get() const { return value; }
};

class Bytes : public Object {
    std::basic_string<unsigned char> value;
public:
    Bytes(TypeRef type, std::basic_string<unsigned char> v);
    std::string to_str() override;
    static TypeRef type;
    std::basic_string<unsigned char> get() const { return value; }
};

class BoundMethod : public Object {
    ObjectRef self, func;
public:
    BoundMethod(TypeRef type, ObjectRef self, ObjectRef func);
    std::string to_str() override;
    static TypeRef type;
    ObjectRef call(const std::vector<ObjectRef>& args) override;
};

class Property : public Object {
    ObjectRef func;
public:
    Property(TypeRef type, ObjectRef func);
    std::string to_str() override;
    static TypeRef type;
    ObjectRef call(const std::vector<ObjectRef>& args) override;
};

using ObjectRefMap = std::unordered_map<ObjectRef, ObjectRef>;

class Dict : public Object {
    ObjectRefMap value;
public:
    Dict(TypeRef type, ObjectRefMap v);
    std::string to_str() override;
    static TypeRef type;
    const ObjectRefMap& get() const { return value; }
};

class List : public Object {
    std::vector<ObjectRef> value;
public:
    List(TypeRef type, std::vector<ObjectRef> v);
    std::string to_str() override;
    static TypeRef type;
    const std::vector<ObjectRef>& get() const { return value; }
};

class Thunk : public Object {
    std::vector<std::shared_ptr<Thunk>> waiting_thunks;
    bool finalized = false;
public:
    Thunk(TypeRef type);
    ~Thunk();
    std::string to_str() override;
    static TypeRef type;
    void subscribe(std::shared_ptr<Thunk> thunk);
    virtual void notify(ObjectRef obj);
    void finalize(ObjectRef obj);
};

#endif // OBJECT_HPP
