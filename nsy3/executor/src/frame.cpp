#include "frame.hpp"

#include "functionutils.hpp"
#include "exception.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>

constexpr unsigned int HALF_INT_MAX = 0xFFFF;

int Frame::execution_debug_level = 0;
TypeRef Frame::type = create<Type>("Frame", Type::basevec{Object::type});

Frame::Frame(TypeRef type, std::shared_ptr<const Code> code, unsigned int offset,
             std::map<std::string, BaseObjectRef> env, unsigned int limit, std::vector<std::pair<unsigned char, ObjectRef>> stack)
    : Object(type), code_(code), position_(offset), limit_(limit), env_(env), stack_(stack) {
    this->env_["__code__"] = code;
}

inline unsigned int stack_push(unsigned char flags, const BaseObjectRef& item, const Frame& frame, const std::basic_string<unsigned char>& code,
                       const std::vector<ObjectRef>& consts, std::vector<std::pair<unsigned char, ObjectRef>>& stack,
                       unsigned int& position, std::map<std::string, BaseObjectRef>& env, unsigned int skip_position, unsigned int skip_save_stack
                       ) {
    if (auto thunk = dynamic_cast<const Thunk*>(item.get())) {
        if (skip_position != 0xFFFF) {
            std::cerr << "Skip from " << position << " to " << skip_position << std::endl;
            std::vector<std::pair<unsigned char, ObjectRef>> sf_stack;
            sf_stack.reserve(stack.size() - skip_save_stack);
            for (auto iter = stack.begin() + skip_save_stack; iter != stack.end(); ++iter) {
                sf_stack.emplace_back(*iter);
            }
            stack.erase(stack.begin() + skip_save_stack, stack.end());
            auto subframe = create<Frame>(frame.code(), position, env, skip_position, sf_stack);

            auto exec_thunk = std::make_shared<ExecutionThunk>(thunk->execution_engine(), subframe);
            thunk->subscribe(exec_thunk);
            // Exploit the fact that skip_pos > pos and that nothing is going to jump out of that range to find all of the sets.
            for (; position < skip_position; position += 5) {
                if (static_cast<Ops>(code[position]) == Ops::SET) {
                    auto arg = *reinterpret_cast<const unsigned int*>(code.data() + position + 1);
                    auto name = dynamic_cast<const String*>(consts[arg].get());
                    if (!name) {
                        create<TypeException>("Name must be a string")->raise();
                    }
                    auto name_thunk = std::make_shared<NameExtractThunk>(thunk->execution_engine(), name->get());
                    exec_thunk->subscribe(name_thunk);
                    env[name->get()] = name_thunk;
                }
            }
            return skip_position;
        }
        else {
            std::cerr << "Skip from " << position << " to return" << std::endl;
            auto subframe = create<Frame>(frame.code(), position, env, skip_position, stack);
            auto exec_thunk = std::make_shared<ExecutionThunk>(thunk->execution_engine(), subframe);
            auto name_thunk = std::make_shared<NameExtractThunk>(thunk->execution_engine(), "return");
            exec_thunk->subscribe(name_thunk);
            thunk->subscribe(exec_thunk);
            env["return"] = name_thunk;
            return static_cast<unsigned int>(-1);
        }
    }
    else {
        stack.emplace_back(std::make_pair(flags, std::dynamic_pointer_cast<const Object>(item)));
    }
    return position;
}

std::map<std::string, BaseObjectRef> Frame::execute() const {
    const auto& code = code_->code;
    const auto& consts = code_->consts;
    auto stack = stack_;
    auto position = position_;
    auto env = env_;
    unsigned int skip_position = limit_, skip_save_stack = 0;


    if (execution_debug_level >= 1) {
        std::cerr << "BEGIN EXEC " << position << " - " << limit_ << std::endl;
    }

    while (position < limit_) {
        auto op = static_cast<Ops>(code[position]);
        auto arg = *reinterpret_cast<const unsigned int*>(code.data() + position + 1);
        if (execution_debug_level >= 1) {
            std::cerr << "S@" << position << std::endl;
            for (auto& obj : stack) {
                std::cerr << " - " << obj.second << std::endl;
            }
            std::cerr << "SD" << std::endl;
        }
        position += 5;
        switch (op) {
            case Ops::KWARG: {
                throw std::runtime_error("IMPL");
                break;
            }
            case Ops::GETATTR: {
                auto nameobj = stack.back().second;
                stack.pop_back();
                auto name = dynamic_cast<const String*>(nameobj.get());
                if (!name) {
                    create<TypeException>("Attribute must be a string")->raise();
                }
                auto obj = stack.back().second;
                stack.pop_back();
                position = stack_push(0, obj->getattr(name->get()), *this, code, consts, stack, position, env, skip_position, skip_save_stack);
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
                position = stack_push(0, func->call(args), *this, code, consts, stack, position, env, skip_position, skip_save_stack);
                break;
            }
            case Ops::BINOP: {
                auto right = stack.back().second;
                stack.pop_back();
                auto left = stack.back().second;
                stack.pop_back();
                auto op = convert<std::string>(consts[arg]);
                BaseObjectRef res;
                try {
                    res = left->gettype(op)->call({right});
                }
                catch (const ObjectRef& exc) {
                    if (dynamic_cast<const UnsupportedOperation*>(exc.get())) {
                        try {
                            res = right->gettype("r" + op)->call({left});
                        }
                        catch (const ObjectRef& exc2) {
                            if (dynamic_cast<const UnsupportedOperation*>(exc2.get())) {
                                throw exc;
                            }
                            throw;
                        }
                    }
                    else {
                        throw;
                    }
                }
                position = stack_push(0, res, *this, code, consts, stack, position, env, skip_position, skip_save_stack);
                break;
            }
            case Ops::GET: {
                auto name = dynamic_cast<const String*>(consts[arg].get());
                if (!name) {
                    create<TypeException>("Name must be a string")->raise();
                }
                auto iter = env.find(name->get());
                if (iter == env.end()) {
                    create<NameException>("Name '" + name->get() + "' is not defined")->raise();
                }
                position = stack_push(0, iter->second, *this, code, consts, stack, position, env, skip_position, skip_save_stack);
                break;
            }
            case Ops::SET: {
                auto name = dynamic_cast<const String*>(consts[arg].get());
                if (!name) {
                    create<TypeException>("Name must be a string")->raise();
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
            case Ops::JUMP_IF: {
                auto obj = stack.back().second;
                stack.pop_back();
                if (obj->to_bool()) {
                    position = arg;
                }
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
            case Ops::JUMP_IF_KEEP: {
                auto obj = stack.back().second;
                if (obj->to_bool()) {
                    position = arg;
                }
                break;
            }
            case Ops::JUMP_IFNOT_KEEP: {
                auto obj = stack.back().second;
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
                env["return"] = obj;
                return env;
            }
            case Ops::GETENV: {
                stack.emplace_back(std::make_pair(0, create<Env>(env)));
                break;
            }
            case Ops::SETSKIP: {
                skip_position = arg & 0xFFFF;
                skip_save_stack = arg >> 16;
                break;
            }
            case Ops::DUP: {
                auto obj = stack.back().second;
                for (auto i = 0u; i < arg; ++i) {
                    stack.emplace_back(std::make_pair(0, obj));
                }
                break;
            }
            case Ops::ROT: {
                auto obj = stack.back();
                stack.pop_back();
                stack.insert(stack.end() - arg, obj);
                break;
            }
            case Ops::RROT: {
                auto obj = (stack.end() - arg - 1)->second;
                stack.erase(stack.end() - arg - 1);
                stack.emplace_back(std::make_pair(0, obj));
                break;
            }
            case Ops::BUILDLIST: {
                std::vector<ObjectRef> args;
                auto pos_iter = stack.end() - arg;
                for (auto iter = pos_iter; iter != stack.end(); ++iter) {
                    args.push_back(iter->second);
                }
                stack.erase(pos_iter, stack.end());
                stack.emplace_back(std::make_pair(0, create<List>(args)));
                break;
            }
            case Ops::UNPACK: {
                auto obj = stack.back().second;
                stack.pop_back();
                if (!dynamic_cast<const List*>(obj.get())) {
                    throw std::runtime_error("Conv iter to list");
                }
                auto lst = std::dynamic_pointer_cast<const List>(obj);
                if ((arg >> 16) == HALF_INT_MAX) {
                    if (lst->get().size() != (arg & HALF_INT_MAX)) {
                        create<ValueException>("Expected sequence of length '" + std::to_string((arg & HALF_INT_MAX))
                                               + "', got '" + std::to_string(lst->get().size()) + "'")->raise();
                    }
                }
                else if (lst->get().size() < (arg & HALF_INT_MAX) - 1) {
                    create<ValueException>("Expected sequence of length '" + std::to_string((arg & HALF_INT_MAX) - 1)
                                           + "' or greater, got '" + std::to_string(lst->get().size()) + "'")->raise();
                }
                auto idx = 0u;
                for (auto i = 0u; i < (arg & HALF_INT_MAX); ++i) {
                    if (i == (arg >> 16)) {
                        std::vector<ObjectRef> subseq(lst->get().begin() + idx, lst->get().end() + (arg >> 16) - (arg & HALF_INT_MAX));
                        stack.emplace_back(std::make_pair(0, create<List>(subseq)));
                        idx += subseq.size();
                    }
                    else {
                        stack.emplace_back(std::make_pair(0, lst->get()[idx++]));
                    }
                }
                break;
            }
            default: {
                throw std::runtime_error("Unrecognized op");
            }
        }
    }
    return env;
}

TypeRef Module::type = create<Type>("Module", Type::basevec{Object::type});

Module::Module(TypeRef type, std::string name, std::map<std::string, BaseObjectRef> v) : Object(type), name(name), value(v) {
}

std::string Module::to_str() const {
    return "Module(" + name + ")";
}

BaseObjectRef Module::getattr(std::string name) const {
    auto iter = value.find(name);
    if (iter == value.end()) {
        return Object::getattr(name);
    }
    return iter->second;
}

TypeRef Env::type = create<Type>("Env", Type::basevec{Object::type});

Env::Env(TypeRef type, std::map<std::string, BaseObjectRef> v) : Object(type), value(v) {
}

ExecutionThunk::ExecutionThunk(ExecutionEngine* execengine, std::shared_ptr<const Frame> frame) : Thunk(execengine), frame(frame) {
}

void ExecutionThunk::notify(BaseObjectRef obj) const {
    if (auto thunk = dynamic_cast<const Thunk*>(obj.get())) {
        std::cerr << "Resubscribe!" << std::endl;
        thunk->subscribe(std::dynamic_pointer_cast<const Thunk>(shared_from_this()));
        return;
    }
    auto new_stack = frame->stack_;
    new_stack.push_back(std::make_pair(0, std::dynamic_pointer_cast<const Object>(obj)));
    auto new_frame = create<Frame>(frame->code_, frame->position_, frame->env_, frame->limit_, std::move(new_stack));
    finalize(create<Env>(new_frame->execute()));
}

std::string ExecutionThunk::to_str() const {
    std::stringstream ss;
    ss << "ET(" << frame->position_ << "-" << frame->limit_ << ")";
    return ss.str();
}


NameExtractThunk::NameExtractThunk(ExecutionEngine* execengine, std::string name) : Thunk(execengine), name(name) {
}

void NameExtractThunk::notify(BaseObjectRef obj) const {
    auto env = std::dynamic_pointer_cast<const Env>(obj);
    auto iter = env->get().find(name);
    if (iter != env->get().end()) {
        finalize(iter->second);
    }
    else {
        finalize(NoneType::none);
    }
}

std::string NameExtractThunk::to_str() const {
    std::stringstream ss;
    ss << "NT(" << name << ")";
    return ss.str();
}
