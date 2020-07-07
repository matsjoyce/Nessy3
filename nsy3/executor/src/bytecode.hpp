#ifndef BYTECODE_HPP
#define BYTECODE_HPP

#include <string>
#include <vector>

#include "object.hpp"


enum class Ops : char {
    KWARG,
    GETATTR,
    CALL,
    GET,
    SET,
    CONST,
    JUMP,
    JUMP_NOTIF,
    DROP,
    RETURN,
    GETENV
};

class Code : public Object {
    std::basic_string<unsigned char> code;
    std::vector<ObjectRef> consts;

public:
    Code(std::basic_string<unsigned char> code, std::vector<ObjectRef> consts);
    static const unsigned int npos = -1;
    static std::shared_ptr<Code> from_file(std::string fname);

    void print();
    std::string to_str() override;

    friend class Frame;
};

class Function : public Object {
    std::shared_ptr<Code> code;
    int offset;
    std::map<std::string, ObjectRef> env;
public:
    Function(std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env);
    ObjectRef call(const std::vector<ObjectRef>& args) override;
    std::string to_str() override;
};

#endif // BYTECODE_HPP
