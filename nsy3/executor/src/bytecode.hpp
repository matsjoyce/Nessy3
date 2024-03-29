#ifndef BYTECODE_HPP
#define BYTECODE_HPP

#include <string>
#include <vector>

#include "object.hpp"


enum class Ops : unsigned char {
    KWARG,
    GETATTR,
    CALL,
    BINOP,
    GET,
    SET,
    CONST,
    JUMP,
    JUMP_IF,
    JUMP_IFNOT,
    JUMP_IF_KEEP,
    JUMP_IFNOT_KEEP,
    DROP,
    RETURN,
    GETENV,
    SETSKIP,
    DUP,
    ROT,
    RROT,
    BUILDLIST,
    UNPACK,
    SKIPVAR
};

class Code : public Object {
    std::basic_string<unsigned char> code;
    std::vector<ObjectRef> consts;
    std::string fname, modulename_;
    std::basic_string<unsigned char> linenotab;

public:
    Code(TypeRef type, std::basic_string<unsigned char> code, std::vector<ObjectRef> consts, std::string fname, std::basic_string<unsigned char> linenotab);
    Code(TypeRef type, ObjectRef header, ObjectRef body);
    static const unsigned int npos = -1;
    static std::shared_ptr<const Code> from_file(std::string fname);
    static std::shared_ptr<const Code> from_string(std::string code);

    void print(std::ostream& stream) const;
    static TypeRef type;
    unsigned int lineno_for_position(unsigned int position) const;
    std::string filename() const;
    std::string modulename() const;

    friend class Frame;
};

class Signature : public Object {
    std::vector<std::string> names;
    std::vector<ObjectRef> defaults;
    unsigned char flags;
public:
    Signature(TypeRef type, std::vector<std::string> names, std::vector<ObjectRef> defaults, unsigned char flags);
    std::string to_str() const override;
    static TypeRef type;

    enum : char {
        VARARGS,
        VARKWARGS
    };

    friend class Function;
};

class Function : public Object {
    std::shared_ptr<const Code> code;
    int offset;
    std::shared_ptr<const Signature> signature_;
    std::map<std::string, BaseObjectRef> env;
public:
    Function(TypeRef type, std::shared_ptr<const Code> code, int offset, std::shared_ptr<const Signature> signature, std::map<std::string, BaseObjectRef> env);
    BaseObjectRef call(const std::vector<ObjectRef>& args) const override;
    std::string to_str() const override;
    static TypeRef type;
    std::shared_ptr<const Signature> signature() const { return signature_; }
};

std::string get_line_of_file(std::string fname, int lineno, bool trim = false);

#endif // BYTECODE_HPP
