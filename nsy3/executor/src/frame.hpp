#ifndef FRAME_HPP
#define FRAME_HPP

#include <map>
#include <vector>

#include "bytecode.hpp"
#include "object.hpp"

class Frame : public Object {
    std::shared_ptr<const Code> code_;
    unsigned int position_ = 0, limit_ = -1;
    std::map<std::string, ObjectRef> env_;
    std::vector<std::pair<unsigned char, ObjectRef>> stack_;
public:
    Frame(TypeRef type, std::shared_ptr<const Code> code, unsigned int offset,
          std::map<std::string, ObjectRef> env, unsigned int limit=-1, std::vector<std::pair<unsigned char, ObjectRef>> stack={});
    static TypeRef type;

    std::map<std::string, ObjectRef> execute() const;
    ObjectRef get_env(std::string name) const;
    bool complete() const;
    std::shared_ptr<const Code> code() const { return code_; }
    std::map<std::string, ObjectRef> env() const { return env_; }

    friend class ExecutionThunk;
};


class ExecutionThunk : public Thunk {
    std::shared_ptr<const Frame> frame;
public:
    ExecutionThunk(TypeRef type, ExecutionEngine* execengine, std::shared_ptr<const Frame> frame);
    void notify(ObjectRef obj) const override;
    std::string to_str() const override;
};

class NameExtractThunk : public Thunk {
    std::string name;
public:
    NameExtractThunk(TypeRef type, ExecutionEngine* execengine, std::string name);
    void notify(ObjectRef obj) const override;
    std::string to_str() const override;
};

#endif // FRAME_HPP
