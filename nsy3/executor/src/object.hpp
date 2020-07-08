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

class Object : public std::enable_shared_from_this<Object> {
    std::shared_ptr<Type> type_;
public:
    Object(std::shared_ptr<Type> type);
    virtual ~Object() = default;
    virtual std::string to_str() = 0;
    virtual bool to_bool();
//     virtual std::size_t hash();
    ObjectRef type();
    ObjectRef getattr(std::string name);
    virtual ObjectRef call(const std::vector<ObjectRef>& args);
};

std::ostream& operator<<(std::ostream& s, const ObjectRef& obj);

class Type : public Object {
    std::map<std::string, ObjectRef> attrs;
public:
    Type();
    Type(std::map<std::string, ObjectRef> attrs);
    std::string to_str() override;
    ObjectRef get(std::string name);
    ObjectRef call(const std::vector<ObjectRef>& args) override;

    using attrmap = std::map<std::string, ObjectRef>;
};

class Numeric : public Object {
public:
    using Object::Object;
    virtual double to_double() const = 0;
};

class Integer : public Numeric {
protected:
    int value;
    Integer(std::shared_ptr<Type> type, int v);
public:
    Integer(int v);
    std::string to_str() override;
    int get() { return value; }
    double to_double() const override { return value; }
    bool to_bool() override;
};

class Float : public Numeric {
    double value;
public:
    Float(double v);
    std::string to_str() override;
    double get() const { return value; }
    double to_double() const override { return value; }
    bool to_bool() override;
};

class Boolean : public Integer {
public:
    Boolean(bool v);
    std::string to_str() override;
};

class String : public Object {
    std::string value;
public:
    String(std::string v);
    std::string to_str() override;
    std::string get() const { return value; }
};

class BoundMethod : public Object {
    ObjectRef self, func;
public:
    BoundMethod(ObjectRef self, ObjectRef func);
    std::string to_str() override;
    ObjectRef call(const std::vector<ObjectRef>& args) override;
};

class Property : public Object {
    ObjectRef func;
public:
    Property(ObjectRef func);
    std::string to_str() override;
    ObjectRef call(const std::vector<ObjectRef>& args) override;
};

using ObjectRefMap = std::unordered_map<ObjectRef, ObjectRef>;

class Dict : public Object {
    ObjectRefMap value;
public:
    Dict(ObjectRefMap v);
    std::string to_str() override;
    const ObjectRefMap& get() const { return value; }
};

class List : public Object {
    std::vector<ObjectRef> value;
public:
    List(std::vector<ObjectRef> v);
    std::string to_str() override;
    const std::vector<ObjectRef>& get() const { return value; }
};

class Thunk : public Object {
    std::vector<std::shared_ptr<Thunk>> waiting_thunks;
    bool finalized = false;
public:
    Thunk();
    ~Thunk();
    std::string to_str() override;
    void subscribe(std::shared_ptr<Thunk> thunk);
    virtual void notify(ObjectRef obj);
    void finalize(ObjectRef obj);
};

#endif // OBJECT_HPP
