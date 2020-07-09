#ifndef FRAME_HPP
#define FRAME_HPP

#include <map>
#include <vector>

#include "bytecode.hpp"
#include "object.hpp"

class Frame : public Object {
    std::shared_ptr<Code> code;
    unsigned int position = 0, skip_position = -1, limit = -1;
    std::map<std::string, ObjectRef> env;
    std::vector<std::pair<unsigned char, ObjectRef>> stack;
    ObjectRef return_thunk;

    void stack_push(unsigned char flags, ObjectRef item, const std::basic_string<unsigned char>& code, const std::vector<ObjectRef>& consts);
public:
    Frame(TypeRef type, std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env);
    std::string to_str() override;
    static TypeRef type;

    ObjectRef execute();
    void set_env(std::string name, ObjectRef value);
    ObjectRef get_env(std::string name);

    friend class ExecutionThunk;
};


class ExecutionThunk : public Thunk {
    std::shared_ptr<Frame> frame;
public:
    ExecutionThunk(TypeRef type, std::shared_ptr<Frame> frame);
    void notify(ObjectRef obj) override;
};

class NameExtractThunk : public Thunk {
    std::string name;
public:
    NameExtractThunk(TypeRef type, std::string name);
    void notify(ObjectRef obj) override;
};

#endif // FRAME_HPP
