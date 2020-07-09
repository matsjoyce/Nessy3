#include "bytecode.hpp"
#include "serialisation.hpp"
#include "frame.hpp"
#include "builtinfunction.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

TypeRef Code::type = create<Type>("Code");

Code::Code(TypeRef type, std::basic_string<unsigned char> code, std::vector<ObjectRef> consts, std::string fname, std::basic_string<unsigned char> linenotab)
    : Object(type), code(code), consts(consts), fname(fname), linenotab(linenotab) {
}

std::shared_ptr<Code> Code::from_file(std::string fname) {
    std::ifstream f(fname);
    if (!f) {
        throw std::runtime_error("Could not open file");
    }

    std::basic_string<unsigned char> ux;
    {
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto x = buffer.str();
        ux = std::basic_string<unsigned char>(reinterpret_cast<unsigned char*>(x.data()), x.size());
    }
    auto [header, pos] = deserialise(ux, 0);
    auto header_map = convert_from_objref<std::map<std::string, ObjectRef>>::convert(header);
    return create<Code>(
        convert_from_objref<std::basic_string<unsigned char>>::convert(header_map["code"]),
        convert_from_objref<std::vector<ObjectRef>>::convert(header_map["consts"]),
        convert_from_objref<std::string>::convert(header_map["fname"]),
        convert_from_objref<std::basic_string<unsigned char>>::convert(header_map["linenotab"])
    );
}

void Code::print() {
    std::cout << "Compiled from " << fname << std::endl;
    std::cout << "Consts:" << std::endl;
    for (auto i = 0u; i < consts.size(); ++i) {
        std::cout << "  " << i << ": " << consts[i] << std::endl;
    }
    std::cout << std::endl << "Code:" << std::endl;
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
            std::cout << "Line " << lineno << std::endl;
        }
        auto op = code[pos];
        auto arg = *reinterpret_cast<unsigned int*>(code.data() + pos + 1);
        std::cout << "  " << pos << ": ";
        switch (static_cast<Ops>(op)) {
            case Ops::KWARG: std::cout << "KWARG " << arg << std::endl; break;
            case Ops::GETATTR: std::cout << "GETATTR " << arg << std::endl; break;
            case Ops::CALL: std::cout << "CALL " << arg << std::endl; break;
            case Ops::GET: std::cout << "GET " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::SET: std::cout << "SET " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::CONST: std::cout << "CONST " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::JUMP: std::cout << "JUMP " << arg << std::endl; break;
            case Ops::JUMP_IFNOT: std::cout << "JUMP_NOTIF " << arg << std::endl; break;
            case Ops::DROP: std::cout << "DROP " << arg << std::endl; break;
            case Ops::RETURN: std::cout << "RETURN " << arg << std::endl; break;
            case Ops::GETENV: std::cout << "GETENV " << arg << std::endl; break;
            case Ops::SETSKIP: std::cout << "SETSKIP " << arg << std::endl; break;
            default: std::cout << "UNKNOWN " << static_cast<unsigned int>(op) << " " << arg << std::endl; break;
        }
        pos += 5;
    }
}

std::string Code::to_str() {
    return "Code(?)";
}

unsigned int Code::lineno_for_position(unsigned int position) {
    unsigned int lineno = 0, lineno_bcode_pos = 0;
    auto linenotab_iter = linenotab.begin();
    while (position >= lineno_bcode_pos + *linenotab_iter && linenotab_iter != linenotab.end()) {
        lineno_bcode_pos += *linenotab_iter++;
        lineno += static_cast<signed char>(*linenotab_iter++);
    }
    return lineno;
}

std::string Code::filename() {
    return fname;
}


TypeRef Signature::type = create<Type>("Signature", Type::attrmap{
    {"__new__", create<BuiltinFunction>(constructor<Signature, std::vector<std::string>, std::vector<ObjectRef>, unsigned char>())}
});

Signature::Signature(TypeRef type, std::vector<std::string> names, std::vector<ObjectRef> defaults, unsigned char flags)
    : Object(type), names(names), defaults(defaults), flags(flags) {
}

std::string Signature::to_str() {
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

TypeRef Function::type = create<Type>("Function", Type::attrmap{
    {"signature", create<Property>(create<BuiltinFunction>(method(&Function::signature)))}
});

Function::Function(TypeRef type, std::shared_ptr<Code> code, int offset, std::shared_ptr<Signature> signature, std::map<std::string, ObjectRef> env)
    : Object(type), code(code), offset(offset), signature_(signature), env(env) {
}

ObjectRef Function::call(const std::vector<ObjectRef>& args) {
    auto new_env = env;
    if (args.size() > signature_->names.size() || args.size() < signature_->names.size() - signature_->defaults.size()) {
        throw std::runtime_error("Wrong no. args.");
    }
    auto name_iter = signature_->names.begin();
    for (auto arg_iter = args.begin(); arg_iter != args.end(); ++name_iter, ++arg_iter) {
        new_env[*name_iter] = *arg_iter;
    }
    for (; name_iter != signature_->names.end(); ++name_iter) {
        new_env[*name_iter] = signature_->defaults[name_iter - signature_->names.end() + signature_->defaults.size()];
    }
    return create<Frame>(code, offset, new_env)->execute();
}

std::string Function::to_str() {
    return "F(?)";
}
