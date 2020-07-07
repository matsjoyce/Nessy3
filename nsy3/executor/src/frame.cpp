#include "frame.hpp"

#include "builtinfunction.hpp"

#include <stdexcept>
#include <iostream>

Frame::Frame(std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env) : code(code), position(offset * 5), env(env) {
    this->env["__code__"] = code;
}

ObjectRef Frame::get_env(std::string name) {
    auto iter = env.find(name);
    if (iter == env.end()) {
        throw std::runtime_error("No such var");
    }
    return iter->second;
}

void Frame::set_env(std::string name, ObjectRef value) {
    env[name] = value;
}

ObjectRef Frame::execute() {
    const auto& code = this->code->code;
    const auto& consts = this->code->consts;
    while (position < code.size()) {
        auto op = static_cast<Ops>(code[position]);
        auto arg = *reinterpret_cast<const unsigned int*>(code.data() + position + 1);
        position += 5;
        switch (op) {
            case Ops::KWARG: {
                throw std::runtime_error("IMPL");
                break;
            }
            case Ops::GETATTR: {
                auto nameobj = stack.back().second;
                stack.pop_back();
                auto name = dynamic_cast<String*>(nameobj.get());
                if (!name) {
                    throw std::runtime_error("Bad name for getattr");
                }
                auto obj = stack.back().second;
                stack.pop_back();
                stack.emplace_back(std::make_pair(0, obj->getattr(name->get())));
                break;
            }
            case Ops::CALL: {
                std::vector<ObjectRef> args;
                auto pos_iter = stack.end() - arg;
                for (auto iter = pos_iter; iter != stack.end(); ++iter) {
                    args.push_back(iter->second);
                }
                stack.erase(pos_iter, stack.end());
                auto func = stack.back().second;
                stack.pop_back();
                stack.emplace_back(std::make_pair(0, func->call(args)));
                break;
            }
            case Ops::GET: {
                auto name = dynamic_cast<String*>(consts[arg].get());
                if (!name) {
                    throw std::runtime_error("Bad name for get");
                }
                stack.emplace_back(std::make_pair(0, get_env(name->get())));
                break;
            }
            case Ops::SET: {
                auto name = dynamic_cast<String*>(consts[arg].get());
                if (!name) {
                    throw std::runtime_error("Bad name for set");
                }
                env[name->get()] = stack.back().second;
                stack.pop_back();
                break;
            }
            case Ops::CONST: {
                stack.emplace_back(std::make_pair(0, consts[arg]));
                break;
            }
            case Ops::JUMP: {
                position = arg * 5;
                break;
            }
            case Ops::JUMP_NOTIF: {
                auto obj = stack.back().second;
                stack.pop_back();
                throw std::runtime_error("IMPL me plz");
                break;
            }
            case Ops::DROP: {
                for (auto i = 0u; i < arg; ++i) {
                    stack.pop_back();
                }
                break;
            }
            case Ops::RETURN: {
                auto obj = stack.back().second;
                stack.pop_back();
                return obj;
            }
            case Ops::GETENV: {
                ObjectRefMap env_copy;
                for (auto item : env) {
                    env_copy[std::make_shared<String>(item.first)] = item.second;
                }
                stack.emplace_back(std::make_pair(0, std::make_shared<Dict>(env_copy)));
                break;
            }
            default: {
                throw std::runtime_error("Unrecognized op");
            }
        }
    }
    std::cout << stack.size() << " " << env.size() << std::endl;
}
