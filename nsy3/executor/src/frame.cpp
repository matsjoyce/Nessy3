#include "frame.hpp"

#include "builtinfunction.hpp"

#include <stdexcept>
#include <iostream>

auto frame_type = std::make_shared<Type>();

Frame::Frame(std::shared_ptr<Code> code, int offset, std::map<std::string, ObjectRef> env) : Object(frame_type), code(code), position(offset), env(env) {
    this->env["__code__"] = code;
    if (!this->env.count("__exec__")) {
        throw std::runtime_error("No execution engine");
    }
}

std::string Frame::to_str() {
    return "Frame";
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

void Frame::stack_push(unsigned char flags, ObjectRef item, const std::basic_string<unsigned char>& code, const std::vector<ObjectRef>& consts) {
    if (auto thunk = dynamic_cast<Thunk*>(item.get())) {
        if (skip_position) {
            auto subframe = std::make_shared<Frame>(this->code, position, env);
            subframe->limit = skip_position;
            std::swap(stack, subframe->stack);

            auto exec_thunk = std::make_shared<ExecutionThunk>(subframe);
            thunk->subscribe(exec_thunk);
            // Exploit the fact that skip_pos > pos and that nothing is going to jump out of that range to find all of the sets.
            for (; position < skip_position; position += 5) {
                if (static_cast<Ops>(code[position]) == Ops::SET) {
                    auto arg = *reinterpret_cast<const unsigned int*>(code.data() + position + 1);
                    auto name = dynamic_cast<String*>(consts[arg].get());
                    if (!name) {
                        throw std::runtime_error("Bad name for set");
                    }
                    auto name_thunk = std::make_shared<NameExtractThunk>(name->get());
                    exec_thunk->subscribe(name_thunk);
                    env[name->get()] = name_thunk;
                }
            }
            position = skip_position;
        }
        else {
            limit = position;
            std::cout << "RETURN THUNK" << std::endl;
            auto exec_thunk = std::make_shared<ExecutionThunk>(std::dynamic_pointer_cast<Frame>(shared_from_this()));
            auto name_thunk = std::make_shared<NameExtractThunk>("return");
            exec_thunk->subscribe(name_thunk);
            thunk->subscribe(exec_thunk);
            return_thunk = name_thunk;
        }
    }
    else {
        stack.emplace_back(std::make_pair(flags, item));
    }
}


ObjectRef Frame::execute() {
    const auto& code = this->code->code;
    const auto& consts = this->code->consts;
    while (position < limit) {
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
                stack_push(0, obj->getattr(name->get()), code, consts);
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
                stack_push(0, func->call(args), code, consts);
                break;
            }
            case Ops::GET: {
                auto name = dynamic_cast<String*>(consts[arg].get());
                if (!name) {
                    throw std::runtime_error("Bad name for get");
                }
                stack_push(0, get_env(name->get()), code, consts);
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
                position = arg;
                break;
            }
            case Ops::JUMP_IFNOT: {
                auto obj = stack.back().second;
                stack.pop_back();
                if (!obj->to_bool()) {
                    position = arg;
                }
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
                set_env("return", obj);
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
            case Ops::SETSKIP: {
                skip_position = arg;
                break;
            }
            default: {
                throw std::runtime_error("Unrecognized op");
            }
        }
    }
    limit = static_cast<unsigned int>(-1);
    return return_thunk;
}

ExecutionThunk::ExecutionThunk(std::shared_ptr<Frame> frame) : frame(frame) {
}

void ExecutionThunk::notify(ObjectRef obj) {
    if (dynamic_cast<Thunk*>(obj.get())) {
        throw std::runtime_error("Notify was a thunk");
    }
    frame->stack.push_back(std::make_pair(0, obj));
    std::cout << "Resume " << frame->stack.size() << " " << frame->position << " " << frame->limit << std::endl;
    frame->execute();
    finalize(frame);
}

NameExtractThunk::NameExtractThunk(std::string name) : name(name) {
}

void NameExtractThunk::notify(ObjectRef obj) {
    auto frame = std::dynamic_pointer_cast<Frame>(obj);
    finalize(frame->get_env(name));
}
