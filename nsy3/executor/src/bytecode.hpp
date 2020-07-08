#ifndef BYTECODE_HPP
#define BYTECODE_HPP

#include <string>
#include <vector>

#include "object.hpp"


enum class Ops : unsigned char {
    KWARG,
    GETATTR,
    CALL,
    GET,
    SET,
    CONST,
    JUMP,
    JUMP_IFNOT,
    DROP,
    RETURN,
    GETENV,
    SETSKIP
};

class Code : public Object {
    std::basic_string<unsigned char> code;
    std::vector<ObjectRef> consts;
    std::string fname;
    std::basic_string<unsigned char> linenotab;

public:
    Code(std::basic_string<unsigned char> code, std::vector<ObjectRef> consts, std::string fname, std::basic_string<unsigned char> linenotab);
    static const unsigned int npos = -1;
    static std::shared_ptr<Code> from_file(std::string fname);

    void print();
    std::string to_str() override;
    unsigned int lineno_for_position(unsigned int position);
    std::string filename();

    friend class Frame;
};

class Signature : public Object {
    std::vector<std::string> names;
    std::vector<ObjectRef> defaults;
    unsigned char flags;
public:
    Signature(std::vector<std::string> names, std::vector<ObjectRef> defaults, unsigned char flags);
    std::string to_str() override;
    static std::shared_ptr<Type> type;

    enum : char {
        VARARGS,
        VARKWARGS
    };

    friend class Function;
};

class Function : public Object {
    std::shared_ptr<Code> code;
    int offset;
    std::shared_ptr<Signature> signature_;
    std::map<std::string, ObjectRef> env;
public:
    Function(std::shared_ptr<Code> code, int offset, std::shared_ptr<Signature> signature, std::map<std::string, ObjectRef> env);
    ObjectRef call(const std::vector<ObjectRef>& args) override;
    std::string to_str() override;
    std::shared_ptr<Signature> signature() { return signature_; }
};

#endif // BYTECODE_HPP
