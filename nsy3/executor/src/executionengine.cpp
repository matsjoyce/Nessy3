#include "executionengine.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "frame.hpp"
#include "builtins.hpp"

#include <iostream>
#include <sstream>

enum class DollarGetFlags : unsigned int {
    PARTIAL = 1 // Can read undecided values
};

enum class DollarSetFlags : unsigned int {
    MODIFICATION = 1, // Can set after the initial set
    DEFAULT = 2 // Only executed if there is no initial set
};

namespace {
    std::ostream& operator<<(std::ostream& s, const DollarName& v) {
        for (auto iter = v.begin(); iter != v.end();) {
            s << *iter;
            if (++iter != v.end()) {
                s << ".";
            }
        }
        return s;
    }
}

ExecutionEngine::ExecutionEngine() {
    env_additions = {
        {"test_thunk", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::test_thunk))},
        {"import", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::import_))},
        {"$?", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::dollar_get))},
        {"$=", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::dollar_set))},
        {"alias", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::make_alias))}
    };
}

ObjectRef ExecutionEngine::import_(std::string name) {
    return modules.at(name);
}

BaseObjectRef ExecutionEngine::test_thunk(std::string name) {
    auto tt = std::make_shared<TestThunk>(this, name);
    state.test_thunks.push_back(tt);
    return tt;
}

BaseObjectRef ExecutionEngine::dollar_get(DollarName name, unsigned int flags) {
    name = dealias(name);
    auto iter = state.dollar_values.find(name);
    if (iter != state.dollar_values.end()) {
        return iter->second;
    }
    std::cerr << "Making get for " << name << std::endl;
    auto thunk = std::make_shared<GetThunk>(this, name, flags);
    state.get_thunks.insert({name, thunk});
    return thunk;
}

bool is_prefix_of(const DollarName& pref, const DollarName& name) {
    if (pref.size() > name.size()) {
        return false;
    }
    for (auto iter_a = pref.begin(), iter_b = name.begin(); iter_a != pref.end(); ++iter_a, ++iter_b) {
        if (*iter_a != *iter_b) {
            return false;
        }
    }
    return true;
}

BaseObjectRef ExecutionEngine::dollar_set(DollarName name, ObjectRef value, unsigned int flags) {
    std::cerr << "Making set for " << name << std::endl;
    name = dealias(name);
    auto thunk = std::make_shared<SetThunk>(this, name, value, flags);
    state.set_thunks.insert({name, thunk});
    return thunk;
}

template<class T> T alias_move(const DollarName& alias, T& m, ExecutionEngine* t) {
    T new_m;
    for (auto iter = m.begin(); iter != m.end();) {
        if (is_prefix_of(alias, iter->first)) {
            new_m.insert({t->dealias(iter->first), std::move(iter->second)});
            iter++;
        }
        else {
            new_m.insert(m.extract(iter++));
        }
    }
    return new_m;
}

void ExecutionEngine::make_alias(DollarName name, DollarName alias) {
    std::cerr << "Alias " << alias << " = " << name << std::endl;
    state.aliases[alias] = name;
    for (auto& item : state.dollar_values) {
        if (is_prefix_of(alias, item.first)) {
            // Cause conflict deliberately
            state.set_thunks.insert({item.first, std::make_shared<SetThunk>(this, item.first, item.second, 0)});
            return;
        }
    }

    state.get_thunks = alias_move(alias, state.get_thunks, this);
    state.set_thunks = alias_move(alias, state.set_thunks, this);
}

DollarName ExecutionEngine::dealias(const DollarName& name) {
    DollarName fixed_name;
    for (auto& item : name) {
        fixed_name.push_back(item);
        auto iter = state.aliases.find(fixed_name);
        while (iter != state.aliases.end()) {
            fixed_name = iter->second;
            iter = state.aliases.find(fixed_name);
        }
    }
    return fixed_name;
}

void ExecutionEngine::finish() {
    initial_state = state;
    while (state.thunk_results.size() || state.get_thunks.size() || state.set_thunks.size()) {
        if (state.set_thunks.size()) {
            auto picked_name = pick_next_dollar_name();

            std::cerr << "Resolving " << picked_name << std::endl;

            resolve_dollar(picked_name);

            std::cerr << "Done." << std::endl;
        }
        for (auto iter = state.get_thunks.begin(); iter != state.get_thunks.end();) {
            auto diter = state.dollar_values.find(iter->first);
            if (diter != state.dollar_values.end()) {
                iter->second->finalize(diter->second);
                state.get_thunks.erase(iter++);
            }
            else {
                ++iter;
            }
        }
        notify_thunks();
        check_consistency();
    }
    while (state.test_thunks.size() || state.thunk_results.size()) {
        while (state.test_thunks.size()) {
            auto tt = state.test_thunks.back();
            state.test_thunks.pop_back();
            tt->finalize(create<Integer>(1));
        }
        notify_thunks();
    }
}

DollarName ExecutionEngine::pick_next_dollar_name() {
    for (auto item : state.set_thunks) {
        if (item.second->flags & ~static_cast<unsigned int>(DollarSetFlags::DEFAULT)) {
            continue;
        }
        auto before = ordering.equal_range(item.first);
        bool ok = true;
        for (auto iter = before.first; iter != before.second; ++iter) {
            if (!state.dollar_values.count(iter->second)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return item.first;
        }
    }
    throw std::runtime_error("Cannot find next dollar name");
}

void ExecutionEngine::resolve_dollar(DollarName name) {
    state.resolution_order.push_back(name);
    ObjectRef value;

    {
        auto iters = state.set_thunks.equal_range(name);
        bool has_nondefault_set = false;
        for (auto iter = iters.first; iter != iters.second;) {
            if (!iter->second->flags) {
                std::cerr << "$Exec " << iter->second->to_str() << std::endl;
                if (has_nondefault_set) {
                    throw std::runtime_error("Multiple non-default initial sets");
                }
                value = iter->second->value;
                iter->second->finalize(create<Integer>(1));
                state.set_thunks.erase(iter++);
                has_nondefault_set = true;
            }
            else if (iter->second->flags & static_cast<unsigned int>(DollarSetFlags::DEFAULT)) {
                std::cerr << "$Exec " << iter->second->to_str() << std::endl;
                if (!has_nondefault_set) {
                    value = iter->second->value;
                }
                iter->second->finalize(create<Integer>(1));
                state.set_thunks.erase(iter++);
            }
            else {
                ++iter;
            }
        }
    }

    {
        while (true) {
            notify_thunks();
            auto set_iter = state.set_thunks.find(name);
            if (set_iter != state.set_thunks.end()) {
                if (!(set_iter->second->flags & static_cast<unsigned int>(DollarSetFlags::MODIFICATION))) {
                    throw std::runtime_error("Non-modification after initial set");
                }
                std::cerr << "$Exec " << set_iter->second->to_str() << std::endl;
                value = set_iter->second->value;
                set_iter->second->finalize(create<Integer>(1));
                state.set_thunks.erase(set_iter);
                continue;
            }
            bool found_get = false;
            auto get_iters = state.get_thunks.equal_range(name);
            for (auto iter = get_iters.first; iter != get_iters.second; ++iter) {
                std::cerr << "$Final " << iter->second->to_str() << std::endl;
                if (iter->second->flags & static_cast<unsigned int>(DollarGetFlags::PARTIAL)) {
                    iter->second->finalize(value);
                    state.get_thunks.erase(iter);
                    found_get = true;
                    break;
                }
            }
            if (found_get) {
                continue;
            }
            break;
        }
    }
    notify_thunks();
    state.dollar_values[name] = value;
    auto get_iters = state.get_thunks.equal_range(name);
    for (auto iter = get_iters.first; iter != get_iters.second; ++iter) {
        std::cerr << "$Final " << iter->second->to_str() << std::endl;
        iter->second->finalize(value);
    }
    state.get_thunks.erase(get_iters.first, get_iters.second);
    notify_thunks();
}

void ExecutionEngine::notify_thunks() {
    // WARNING: Thunk lists are in flux during calls to notify!
    while (state.thunk_results.size()) {
        auto [thunk, res] = state.thunk_results.back();
        state.thunk_results.pop_back();
        std::cerr << "finalizing " << thunk->to_str() << std::endl;
        auto subbed = state.thunk_subscriptions.equal_range(thunk);
        auto subbed_v = std::vector(subbed.first, subbed.second);
        state.thunk_subscriptions.erase(subbed.first, subbed.second);
        for (auto sub : subbed_v) {
            std::cerr << "  notifying " << sub.second->to_str() << std::endl;
            sub.second->notify(res);
        }
    }
}

void ExecutionEngine::check_consistency() {
    for (auto item : state.set_thunks) {
        std::cerr << "Checking " << item.first << std::endl;
        if (state.dollar_values.count(item.first)) {
            std::cerr << "Conflict: " << item.first << " already set, but new set revealed by " << state.resolution_order.back() << std::endl;
            ordering.insert({item.first, state.resolution_order.back()});
            state = initial_state;
            std::cerr << "RESET!" << std::endl;
            std::cerr << "Ordering now has " << ordering.size() << " items" << std::endl;
        }
    }
}

void ExecutionEngine::exec_file(std::string fname) {
    auto c = Code::from_file(fname);
    c->print(std::cerr);
    std::map<std::string, BaseObjectRef> start_env;
    for (auto& item : builtins) {
        start_env[item.first] = item.second;
    }
    for (auto& item : env_additions) {
        start_env[item.first] = item.second;
    }
    std::cerr << "Executing " << fname << std::endl;
    auto frame = create<Frame>(c, 0, start_env);
    auto end_env = frame->execute();
    for (auto& mod : modules) {
        // Startswith
        if (mod.first.rfind(c->modulename() + ".", 0) == 0 && mod.first.rfind(".") == c->modulename().size()) {
            end_env[mod.first.substr(mod.first.rfind(".") + 1)] = mod.second;
        }
    }
    auto module = create<Module>(c->modulename(), end_env);
    modules[c->modulename()] = module;
}

void ExecutionEngine::exec_runspec(ObjectRef runspec) {
    auto runspec_dict = convert_ptr<Dict>(runspec);
    for (auto item : convert_ptr<List>(runspec_dict->get().at(create<String>("files")))->get()) {
        exec_file(convert<std::string>(item));
    }
    std::cerr << "Initial execution done" << std::endl;
}

void ExecutionEngine::subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest) {
    state.thunk_subscriptions.insert({source, dest});
}

void ExecutionEngine::finalize_thunk(std::shared_ptr<const Thunk> source, BaseObjectRef result) {
    state.thunk_results.push_back({source, result});
}

TestThunk::TestThunk(ExecutionEngine* execengine, std::string name) : Thunk(execengine), name(name) {
}

std::string TestThunk::to_str() const {
    return "TT(" + name + ")";
}

GetThunk::GetThunk(ExecutionEngine* execengine, DollarName name, unsigned int flags)
    : Thunk(execengine), name(name), flags(flags) {
}

std::string GetThunk::to_str() const {
    std::stringstream ss;
    ss << "GT(" << name << "@" << flags << ")";
    return ss.str();
}

SetThunk::SetThunk(ExecutionEngine* execengine, DollarName name, ObjectRef value, unsigned int flags)
    : Thunk(execengine), name(name), value(value), flags(flags) {
}

std::string SetThunk::to_str() const {
    std::stringstream ss;
    ss << "ST(" << name << "=" << value << " @" << flags << ")";
    return ss.str();
}
