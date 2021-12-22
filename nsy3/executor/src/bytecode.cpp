#include "bytecode.hpp"
#include "serialisation.hpp"
#include "frame.hpp"
#include "functionutils.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

TypeRef Code::type = create<Type>("Code", Type::basevec{Object::type});

Code::Code(TypeRef type, std::basic_string<unsigned char> code, std::vector<ObjectRef> consts, std::string fname, std::basic_string<unsigned char> linenotab)
    : Object(type), code(code), consts(consts), fname(fname), linenotab(linenotab) {
}

std::shared_ptr<const Code> Code::from_file(std::string fname) {
    std::ifstream f(fname);
    if (!f) {
        throw std::runtime_error("Could not open file");
    }

    auto header = deserialise_from_file(f);
    auto body = deserialise_from_file(f);
    return create<Code>(header, body);
}

std::shared_ptr<const Code> Code::from_string(std::string code) {
    std::stringstream f(code);
    auto header = deserialise_from_file(f);
    auto body = deserialise_from_file(f);
    return create<Code>(header, body);
}

Code::Code(TypeRef type, ObjectRef header, ObjectRef body) : Object(type) {
    auto header_map = convert<std::map<std::string, ObjectRef>>(header);
    auto body_map = convert<std::map<std::string, ObjectRef>>(body);
    code = convert<std::basic_string<unsigned char>>(body_map["code"]);
    consts = convert<std::vector<ObjectRef>>(body_map["consts"]);
    fname = convert<std::string>(header_map["fname"]);
    linenotab = convert<std::basic_string<unsigned char>>(body_map["linenotab"]);
    modulename_ = convert<std::string>(header_map["name"]);
}

void Code::print(std::ostream& stream) const {
    stream << "Compiled from " << fname << " (" << modulename_ << ")\n";
    stream << "Consts:\n";
    for (auto i = 0u; i < consts.size(); ++i) {
        stream << "  " << i << ": " << consts[i] << "\n";
    }
    stream << "\nCode:\n";
    unsigned int pos = 0;
    unsigned int lineno = 0, lineno_bcode_pos = 0;
    auto linenotab_iter = linenotab.begin();
    bool lineno_change = false;
    while (pos < code.size()) {
        lineno_change = false;
        while (pos >= lineno_bcode_pos + *linenotab_iter && linenotab_iter != linenotab.end()) {
            lineno_bcode_pos += *linenotab_iter++;
            lineno += static_cast<signed char>(*linenotab_iter++);
            lineno_change = true;
        }
        if (lineno_change) {
            stream << "Line " << lineno << ": " << get_line_of_file(fname, lineno, true) << "\n";
        }
        auto op = code[pos];
        auto arg = *reinterpret_cast<const unsigned int*>(code.data() + pos + 1);
        stream << "  " << pos << ": ";
        switch (static_cast<Ops>(op)) {
            case Ops::KWARG: stream << "KWARG " << arg << "\n"; break;
            case Ops::GETATTR: stream << "GETATTR " << arg << "\n"; break;
            case Ops::CALL: stream << "CALL " << arg << "\n"; break;
            case Ops::BINOP: stream << "BINOP " << arg << " (" << consts[arg] << ")\n"; break;
            case Ops::GET: stream << "GET " << arg << " (" << consts[arg] << ")\n"; break;
            case Ops::SET: stream << "SET " << arg << " (" << consts[arg] << ")\n"; break;
            case Ops::CONST: stream << "CONST " << arg << " (" << consts[arg] << ")\n"; break;
            case Ops::JUMP: stream << "JUMP " << arg << "\n"; break;
            case Ops::JUMP_IF: stream << "JUMP_IF " << arg << "\n"; break;
            case Ops::JUMP_IFNOT: stream << "JUMP_IFNOT " << arg << "\n"; break;
            case Ops::JUMP_IF_KEEP: stream << "JUMP_IF_KEEP " << arg << "\n"; break;
            case Ops::JUMP_IFNOT_KEEP: stream << "JUMP_IFNOT_KEEP " << arg << "\n"; break;
            case Ops::DROP: stream << "DROP " << arg << "\n"; break;
            case Ops::RETURN: stream << "RETURN " << arg << "\n"; break;
            case Ops::GETENV: stream << "GETENV " << arg << "\n"; break;
            case Ops::SETSKIP: stream << "SETSKIP " << (arg & 0xFFFF) << " " << (arg >> 16) << "\n"; break;
            case Ops::DUP: stream << "DUP " << arg << "\n"; break;
            case Ops::ROT: stream << "ROT " << arg << "\n"; break;
            case Ops::RROT: stream << "RROT " << arg << "\n"; break;
            case Ops::BUILDLIST: stream << "BUILDLIST " << arg << "\n"; break;
            case Ops::UNPACK: stream << "UNPACK " << (arg & 0xFFFF) << " " << (arg >> 16) << "\n"; break;
            default: stream << "UNKNOWN " << static_cast<unsigned int>(op) << " " << arg << "\n"; break;
        }
        pos += 5;
    }
}

unsigned int Code::lineno_for_position(unsigned int position) const {
    unsigned int lineno = 0, lineno_bcode_pos = 0;
    auto linenotab_iter = linenotab.begin();
    while (position >= lineno_bcode_pos + *linenotab_iter && linenotab_iter != linenotab.end()) {
        lineno_bcode_pos += *linenotab_iter++;
        lineno += static_cast<signed char>(*linenotab_iter++);
    }
    return lineno;
}

std::string Code::filename() const {
    return fname;
}

std::string Code::modulename() const {
    return modulename_;
}

TypeRef Signature::type = create<Type>("Signature", Type::basevec{Object::type}, Type::attrmap{
    {"__new__", create<BuiltinFunction>(constructor<Signature, std::vector<std::string>, std::vector<ObjectRef>, unsigned char>())}
});

Signature::Signature(TypeRef type, std::vector<std::string> names, std::vector<ObjectRef> defaults, unsigned char flags)
    : Object(type), names(names), defaults(defaults), flags(flags) {
}

std::string Signature::to_str() const {
    std::string res;
    auto name_iter = names.rbegin();
    if (flags & VARKWARGS) {
        res = "**" + *name_iter++;
    }
    for (auto def : defaults) {
        if (res.size()) res = ", " + res;
        res = *name_iter++ + "=" + def->to_str() + res;
    }
    if (flags & VARARGS) {
        if (res.size()) res = ", " + res;
        res = "*" + *name_iter++ + res;
    }
    while (name_iter != names.rend()) {
        if (res.size()) res = ", " + res;
        res = *name_iter++ + res;
    }
    return "Signature(" + res + ")";
}

TypeRef Function::type = create<Type>("Function", Type::basevec{Object::type}, Type::attrmap{
    {"signature", create<Property>(create<BuiltinFunction>(method(&Function::signature)))}
});

Function::Function(TypeRef type, std::shared_ptr<const Code> code, int offset, std::shared_ptr<const Signature> signature, std::map<std::string, BaseObjectRef> env)
    : Object(type), code(code), offset(offset), signature_(signature), env(env) {
}

BaseObjectRef Function::call(const std::vector<ObjectRef>& args) const {
    auto new_env = env;
    if (args.size() > signature_->names.size() || args.size() < signature_->names.size() - signature_->defaults.size()) {
        create<ValueError>("Wrong number of arguments")->raise();
    }
    auto name_iter = signature_->names.begin();
    for (auto arg_iter = args.begin(); arg_iter != args.end(); ++name_iter, ++arg_iter) {
        new_env[*name_iter] = *arg_iter;
    }
    for (; name_iter != signature_->names.end(); ++name_iter) {
        new_env[*name_iter] = signature_->defaults[name_iter - signature_->names.end() + signature_->defaults.size()];
    }
    return create<Frame>(code, offset, new_env)->execute()["return"];
}

std::string Function::to_str() const {
    return "F(?)";
}

std::string ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string get_line_of_file(std::string fname, int lineno, bool trim) {
    std::string res;
    std::ifstream f(fname);
    int current_line = 1;
    while (f) {
        std::getline(f, res);
        if (current_line == lineno) {
            return trim ? ltrim(res) : res;
        }
        ++current_line;
    }
    res.clear();
    return res;
}
