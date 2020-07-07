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
    void add(std::string name, ObjectRef obj);

    using attrmap = std::map<std::string, ObjectRef>;
};

class Numeric : public Object {
public:
    using Object::Object;
    virtual double to_double() const = 0;
};

class Integer : public Numeric {
    int value;
public:
    Integer(int v);
    std::string to_str() override;
    int get() { return value; }
    virtual double to_double() const override { return value; }
};

class Float : public Numeric {
    double value;
public:
    Float(double v);
    std::string to_str() override;
    double get() const { return value; }
    virtual double to_double() const override { return value; }
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

using ObjectRefMap = std::unordered_map<ObjectRef, ObjectRef>;

class Dict : public Object {
    ObjectRefMap value;
public:
    Dict(ObjectRefMap v);
    std::string to_str() override;
    const ObjectRefMap& get() const { return value; }
};

#endif // OBJECT_HPP
