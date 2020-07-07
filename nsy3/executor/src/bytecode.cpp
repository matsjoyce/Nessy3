#include "bytecode.hpp"
#include "serialisation.hpp"
#include "frame.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

auto code_type = std::make_shared<Type>();

Code::Code(std::basic_string<unsigned char> code, std::vector<ObjectRef> consts) : Object(code_type), code(code), consts(consts) {
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
    auto num_consts = *reinterpret_cast<unsigned int*>(ux.data());
    auto pos = 4u;
    std::vector<ObjectRef> consts;
    for (auto i = 0u; i < num_consts; ++i) {
        auto [obj, new_pos] = deserialise(ux, pos);
        consts.push_back(obj);
        pos = new_pos;
        std::cout << obj << std::endl;
    }
    return std::make_shared<Code>(ux.substr(pos), consts);
}

void Code::print() {
    unsigned int pos = 0;
    while (pos < code.size()) {
        auto op = code[pos];
        auto arg = *reinterpret_cast<unsigned int*>(code.data() + pos + 1);
        switch (static_cast<Ops>(op)) {
            case Ops::KWARG: std::cout << "KWARG " << arg << std::endl; break;
            case Ops::GETATTR: std::cout << "GETATTR " << arg << std::endl; break;
            case Ops::CALL: std::cout << "CALL " << arg << std::endl; break;
            case Ops::GET: std::cout << "GET " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::SET: std::cout << "SET " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::CONST: std::cout << "CONST " << arg << " (" << consts[arg] << ")" << std::endl; break;
            case Ops::JUMP: std::cout << "JUMP " << arg << std::endl; break;
            case Ops::JUMP_NOTIF: std::cout << "JUMP_NOTIF " << arg << std::endl; break;
            case Ops::DROP: std::cout << "DROP " << arg << std::endl; break;
            case Ops::RETURN: std::cout << "RETURN " << arg << std::endl; break;
            case Ops::GETENV: std::cout << "GETENV " << arg << std::endl; break;
            default: std::cout << "UNKNOWN " << static_cast<unsigned int>(op) << " " << arg << std::endl; break;
        }
        pos += 5;
    }
}

std::string Code::to_str() {
    return "Code(?)";
}

auto function_type = std::make_shared<Type>();

Function::Function(std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env) : Object(function_type), code(code), offset(offset), env(env) {
}

ObjectRef Function::call(const std::vector<ObjectRef>& args) {
    return Frame(code, offset, env).execute();
}

std::string Function::to_str() {
    return "F(?)";
}


