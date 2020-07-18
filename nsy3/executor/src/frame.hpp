#ifndef FRAME_HPP
#define FRAME_HPP

#include <map>
#include <vector>

#include "bytecode.hpp"
#include "object.hpp"
#include "thunk.hpp"

class Frame : public Object {
    std::shared_ptr<const Code> code_;
    unsigned int position_ = 0, limit_ = -1;
    std::map<std::string, BaseObjectRef> env_;
    std::vector<std::pair<unsigned char, ObjectRef>> stack_;
public:
    Frame(TypeRef type, std::shared_ptr<const Code> code, unsigned int offset,
          std::map<std::string, BaseObjectRef> env, unsigned int limit=-1, std::vector<std::pair<unsigned char, ObjectRef>> stack={});
    static TypeRef type;

    std::map<std::string, BaseObjectRef> execute() const;
    bool complete() const;
    std::shared_ptr<const Code> code() const { return code_; }
    std::map<std::string, BaseObjectRef> env() const { return env_; }

    static int execution_debug_level;

    friend class ExecutionThunk;
};

class Module : public Object {
    std::string name;
    std::map<std::string, BaseObjectRef> value;
public:
    Module(TypeRef type, std::string name, std::map<std::string, BaseObjectRef> v);
    std::string to_str() const override;
    static TypeRef type;
    BaseObjectRef getattr(std::string name) const override;
};

class Env : public Object {
    std::map<std::string, BaseObjectRef> value;
public:
    Env(TypeRef type, std::map<std::string, BaseObjectRef> v);
    static TypeRef type;
    const std::map<std::string, BaseObjectRef>& get() const { return value; }
};

class ExecutionThunk : public Thunk {
    std::shared_ptr<const Frame> frame;
public:
    ExecutionThunk(ExecutionEngine* execengine, std::shared_ptr<const Frame> frame);
    void notify(BaseObjectRef obj) const override;
    std::string to_str() const override;
};

class NameExtractThunk : public Thunk {
    std::string name;
public:
    NameExtractThunk(ExecutionEngine* execengine, std::string name);
    void notify(BaseObjectRef obj) const override;
    std::string to_str() const override;
};

#endif // FRAME_HPP
