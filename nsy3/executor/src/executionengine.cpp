#include "executionengine.hpp"
#include "executionengine_private.hpp"
#include "functionutils.hpp"
#include "bytecode.hpp"
#include "frame.hpp"
#include "builtins.hpp"
#include "serialisation.hpp"

#include <iostream>
#include <sstream>

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
        {"alias", create<BuiltinFunction>(method_and_bind(this, &ExecutionEngine::make_alias))},
        {"subs", create<BuiltinFunction>([this](DollarName name) {
            return create<SubIter>(this, name);
        })}
    };
}

BaseObjectRef ExecutionEngine::import_(std::string name) {
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
    state.get_thunks[name].push_back(thunk);
    return thunk;
}

BaseObjectRef ExecutionEngine::make_sub_thunk(DollarName name, unsigned int position) {
    auto thunk = std::make_shared<SubThunk>(this, name, position);
    state.sub_thunks[name].push_back(thunk);
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
    state.set_thunks[name].push_back(thunk);
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

BaseObjectRef ExecutionEngine::make_alias(DollarName name, DollarName alias) {
    std::cerr << "Alias " << alias << " = " << name << std::endl;
    state.aliases[alias] = name;
    for (auto& item : state.dollar_values) {
        if (is_prefix_of(alias, item.first)) {
            // Cause conflict deliberately
            state.set_thunks[item.first].push_back(std::make_shared<SetThunk>(this, item.first, item.second, 0));
            return NoneType::none;
        }
    }

    state.get_thunks = alias_move(alias, state.get_thunks, this);
    state.set_thunks = alias_move(alias, state.set_thunks, this);
    return NoneType::none;
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
    while (state.get_thunks.size() || state.set_thunks.size() || state.sub_thunks.size()) {
        bool done_something = false;
        if (state.set_thunks.size()) {
            auto picked_name = pick_next_dollar_name();
            if (picked_name.size()) {
                std::cerr << "Resolving " << picked_name << std::endl;

                resolve_dollar(picked_name);

                std::cerr << "Done." << std::endl;
                done_something = true;
            }
        }
        // Lack of short-circuiting is deliberate!
        done_something |= finalize_abandoned_get_thunks();
        done_something |= finalize_abandoned_sub_thunks();
        notify_thunks();
        check_consistency();
        if (!done_something) {
            auto dummy_name = pick_dummy_name();
            resolve_dummy(dummy_name);
        }
    }
    while (state.test_thunks.size()) {
        while (state.test_thunks.size()) {
            auto tt = state.test_thunks.back();
            std::cerr << "Resolving " << tt->to_str() << std::endl;
            state.test_thunks.pop_back();
            tt->finalize(create<Integer>(1));
        }
        notify_thunks();
    }

    std::cerr << "Finished!" << std::endl;
    for (auto& dn : state.dollar_values) {
        std::cerr << dn.first << " = " << dn.second << std::endl;
    }
    std::cerr << "Resolved in " << resets << " resets" << std::endl;
    std::cout << "=== MARKER ===" << std::endl;
    ObjectRefMap m;
    for (auto& dn : state.dollar_values) {
        if (!dn.second) {
            continue;
        }
        std::stringstream ss;
        ss << dn.first;
        m[create<String>(ss.str())] = dn.second;
    }
    serialize_to_file(std::cout, create<Dict>(m));
    std::cout << "=== END MARKER ===" << std::endl;
}

bool ExecutionEngine::finalize_abandoned_get_thunks() {
    bool done_something = false;
    for (auto iter = state.get_thunks.begin(); iter != state.get_thunks.end();) {
        auto diter = state.dollar_values.find(iter->first);
        if (diter != state.dollar_values.end()) {
            for (auto& thunk : iter->second) {
                thunk->finalize(diter->second);
            }
            state.get_thunks.erase(iter++);
            done_something = true;
        }
        else {
            ++iter;
        }
    }
    return done_something;
}

bool ExecutionEngine::finalize_abandoned_sub_thunks() {
    bool done_something = false;
    for (auto iter = state.sub_thunks.begin(); iter != state.sub_thunks.end();) {
        bool resolved = state.dollar_values.find(iter->first) != state.dollar_values.end();
        auto sub_names_iter = state.sub_names.find(iter->first);
        if (sub_names_iter == state.sub_names.end()) {
            if (resolved) {
                // No more coming
                for (auto& thunk : iter->second) {
                    thunk->finalize(NoneType::none);
                }
                state.sub_thunks.erase(iter++);
                done_something = true;
            }
            else {
                // Maybe more, leave it
                ++iter;
            }
        }
        else {
            for (auto thunk_iter = iter->second.begin(); thunk_iter != iter->second.end();) {
                if (sub_names_iter->second.size() > (*thunk_iter)->position) {
                    // Got some more
                    (*thunk_iter)->handle(sub_names_iter->second[(*thunk_iter)->position]);
                    thunk_iter = iter->second.erase(thunk_iter);
                    done_something = true;
                }
                else if (resolved) {
                    // No more coming
                    (*thunk_iter)->finalize(NoneType::none);
                    thunk_iter = iter->second.erase(thunk_iter);
                    done_something = true;
                }
                else {
                    // Maybe more, leave it
                    ++thunk_iter;
                }
            }
            // Clean up empty vectors
            if (iter->second.size()) {
                ++iter;
            }
            else {
                state.sub_thunks.erase(iter++);
            }
        }
    }
    return done_something;
}

DollarName ExecutionEngine::pick_next_dollar_name() {
    for (auto item : state.set_thunks) {
        bool found_initial = false;
        for (auto& thunk : item.second) {
            if (!(thunk->flags & ~static_cast<unsigned int>(DollarSetFlags::DEFAULT))) {
                found_initial = true;
                break;
            }
        }
        if (!found_initial) {
            continue;
        }
        bool ok = true;
        for (auto& name : ordering[item.first]) {
            if (!state.dollar_values.count(name)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return item.first;
        }
    }
    return {};
}

DollarName ExecutionEngine::pick_dummy_name() {
    std::vector<DollarName> to_check;
    for (auto& item : state.set_thunks) {
        to_check.push_back(item.first);
    }
    for (auto& item : state.sub_thunks) {
        to_check.push_back(item.first);
        std::cerr << "DN " << item.first << std::endl;
    }
    while (to_check.size()) {
        auto check = std::move(to_check.back());
        to_check.pop_back();
        bool ok = state.set_thunks.find(check) == state.set_thunks.end();
        if (ok) {
            for (auto& name : ordering[check]) {
                if (!state.dollar_values.count(name)) {
                    ok = false;
                    to_check.push_back(name);
                }
            }
        }
        if (ok) {
            return check;
        }
    }
    std::cerr << "Cannot make progress!" << std::endl;
    {
        std::vector<DollarName> to_check;
        for (auto& item : state.set_thunks) {
            to_check.push_back(item.first);
        }
        for (auto& item : state.sub_thunks) {
            to_check.push_back(item.first);
        }
        while (to_check.size()) {
            auto check = std::move(to_check.back());
            to_check.pop_back();
            std::cerr << check << ": ";
            if (state.set_thunks.find(check) == state.set_thunks.end()) {
                std::cerr << "Has no sets; ";
            }
            else {
                std::cerr << "Has sets; ";
            }
            for (auto& name : ordering[check]) {
                if (!state.dollar_values.count(name)) {
                    std::cerr << "Waiting for " << name << ";";
                    to_check.push_back(name);
                }
            }
            std::cerr << std::endl;
        }
        for (auto& item : state.get_thunks) {
            std::cerr << "Get(s) for " << item.first << std::endl;
        }
    }
    for (auto& dn : state.dollar_values) {
        std::cerr << dn.first << " = " << dn.second << std::endl;
    }
    std::cerr << "Took " << resets << " resets" << std::endl;
    throw std::runtime_error("Cannot make progress!");
}

void ExecutionEngine::resolve_dollar(DollarName name) {
    state.resolution_order.push_back(name);
    ObjectRef value;

    // Update sub iterators
    DollarName parent = {name[0]};
    for (auto iter = ++name.begin(); iter != name.end(); ++iter) {
        auto& pnames = state.sub_names[parent];
        auto siter = std::find(pnames.begin(), pnames.end(), *iter);
        if (siter == pnames.end()) {
            pnames.push_back(*iter);
            auto sub_thunk_iter = state.sub_thunks.find(parent);
            if (sub_thunk_iter != state.sub_thunks.end()) {
                for (auto thunk_iter = sub_thunk_iter->second.begin(); thunk_iter != sub_thunk_iter->second.end();) {
                    if ((*thunk_iter)->position == pnames.size() - 1) {
                        (*thunk_iter)->handle(*iter);
                        thunk_iter = sub_thunk_iter->second.erase(thunk_iter);
                    }
                    else {
                        ++thunk_iter;
                    }
                }
                if (!sub_thunk_iter->second.size()) {
                    state.sub_thunks.erase(sub_thunk_iter);
                }
            }
        }
        parent.push_back(*iter);
    }

    // Initial value set
    {
        auto& thunks = state.set_thunks[name];
        bool has_nondefault_set = false;
        for (auto iter = thunks.begin(); iter != thunks.end();) {
            if (!(*iter)->flags) {
                std::cerr << "$Exec " << (*iter)->to_str() << std::endl;
                if (has_nondefault_set) {
                    throw std::runtime_error("Multiple non-default initial sets");
                }
                value = (*iter)->value;
                (*iter)->finalize(create<Integer>(1));
                iter = thunks.erase(iter);
                has_nondefault_set = true;
            }
            else if ((*iter)->flags & static_cast<unsigned int>(DollarSetFlags::DEFAULT)) {
                std::cerr << "$Exec " << (*iter)->to_str() << std::endl;
                if (!has_nondefault_set) {
                    value = (*iter)->value;
                }
                (*iter)->finalize(create<Integer>(1));
                iter = thunks.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }

    // Modifying sets
    {
        while (true) {
            std::cerr << "RML" << std::endl;
            notify_thunks();
            auto& set_thunks = state.set_thunks[name];
            if (set_thunks.size()) {
                auto& thunk = set_thunks.front();
                if (!(thunk->flags & static_cast<unsigned int>(DollarSetFlags::MODIFICATION))) {
                    throw std::runtime_error("Non-modification after initial set");
                }
                std::cerr << "$Exec " << thunk->to_str() << std::endl;
                value = thunk->value;
                thunk->finalize(NoneType::none);
                set_thunks.erase(set_thunks.begin());
                continue;
            }
            bool found_get = false;
            auto& get_thunks = state.get_thunks[name];
            for (auto iter = get_thunks.begin(); iter != get_thunks.end();) {
                std::cerr << "$Final " << (*iter)->to_str() << std::endl;
                if ((*iter)->flags & static_cast<unsigned int>(DollarGetFlags::PARTIAL)) {
                    (*iter)->finalize(value);
                    iter = get_thunks.erase(iter);
                    found_get = true;
                    break;
                }
                else {
                     ++iter;
                }
            }
            if (found_get || state.set_thunks[name].size()) {
                continue;
            }
            break;
        }
    }
    std::cerr << "RMLe" << std::endl;
    notify_thunks();
    state.set_thunks.erase(name);

    // Handle gets
    state.dollar_values[name] = value;

    for (auto& thunk : state.get_thunks[name]) {
        std::cerr << "$Final " << thunk->to_str() << std::endl;
        thunk->finalize(value);
    }
    state.get_thunks.erase(name);
    notify_thunks();
}

void ExecutionEngine::resolve_dummy(DollarName name) {
    std::cerr << "Dummy resolving " << name << std::endl;
    state.resolution_order.push_back(name);
    state.dollar_values[name] = {};
}

void ExecutionEngine::notify_thunks() {
    // WARNING: Thunk lists are in flux during calls to notify!

    while (state.thunk_subscriptions.size()) {
        bool done = true;
        for (auto iter = state.thunk_subscriptions.begin(); iter != state.thunk_subscriptions.end();) {
            auto result_iter = state.thunk_results.find(iter->first);
            if (result_iter != state.thunk_results.end()) {
                auto thunks = std::move(iter->second);
                state.thunk_subscriptions.erase(iter++);
                for (auto thunk : thunks) {
                    std::cerr << "  notifying " << thunk->to_str() << std::endl;
                    thunk->notify(result_iter->second);
                }
                done = false;
            }
            else {
                ++iter;
            }
        }
        if (done) {
            break;
        }
    }
}

void ExecutionEngine::check_consistency() {
    for (auto item : state.get_thunks) {
        if (!item.second.size()) {
            throw std::runtime_error("Empty get on the queue");
        }
    }
    for (auto item : state.sub_thunks) {
        if (!item.second.size()) {
            throw std::runtime_error("Empty sub on the queue");
        }
    }
    bool conflict = false;
    for (auto item : state.set_thunks) {
        if (!item.second.size()) {
            throw std::runtime_error("Empty set on the queue");
        }
//         std::cerr << "Checking " << item.first << std::endl;
        if (state.dollar_values.count(item.first)) {
            std::cerr << "Conflict: " << item.first << " already set, but new set revealed by " << state.resolution_order.back() << std::endl;
            if (state.resolution_order.back() == item.first) {
                throw std::runtime_error("Circular self dependency");
            }
            ordering[item.first].push_back(state.resolution_order.back());
            conflict = true;
        }
        DollarName parent(item.first.begin(), --item.first.end());
        if (parent.size() && state.dollar_values.count(parent)) {
            std::cerr << "Conflict: " << parent << " already (dummy) set, but new child " << item.first << " revealed by " << state.resolution_order.back() << std::endl;
            ordering[parent].push_back(state.resolution_order.back());
            conflict = true;
        }
    }
    if (!conflict) {
        return;
    }
    state = initial_state;
    ++resets;
    std::cerr << "RESET!" << std::endl;
    std::cerr << "Ordering now has " << ordering.size() << " items" << std::endl;
    for (auto& item : ordering) {
        std::cerr << "    " << item.first << ": ";
        for (auto& name : item.second) {
            std::cerr << name << ", ";
        }
        std::cerr << std::endl;
    }
}

void ExecutionEngine::exec_code(std::shared_ptr<const Code> code) {
    code->print(std::cerr);
    std::map<std::string, BaseObjectRef> start_env;
    for (auto& item : builtins) {
        start_env[item.first] = item.second;
    }
    for (auto& item : env_additions) {
        start_env[item.first] = item.second;
    }
    std::cerr << "Executing " << code->filename() << std::endl;
    auto frame = create<Frame>(code, 0, start_env);
    auto end_env = frame->execute();
    auto module = create<Module>(code->modulename(), end_env);
    auto thunk_iter = modules.find(code->modulename());
    if (thunk_iter != modules.end()) {
        std::dynamic_pointer_cast<const ModuleThunk>(thunk_iter->second)->finalize(module);
    }
    modules[code->modulename()] = module;
}

void ExecutionEngine::exec_runspec(ObjectRef runspec) {
    auto runspec_dict = convert_ptr<Dict>(runspec);

    for (auto module_ : convert<std::vector<std::string>>(runspec_dict->get().at(create<String>("modules")))) {
        modules[module_] = std::make_shared<ModuleThunk>(this, module_);
    }
    for (auto item : convert_ptr<List>(runspec_dict->get().at(create<String>("files")))->get()) {
        exec_code(Code::from_file(convert<std::string>(item)));
    }
    auto conclusion = runspec_dict->get().at(create<String>("conclusion"));
    if (conclusion != NoneType::none) {
        auto bytes = std::dynamic_pointer_cast<const Bytes>(conclusion)->get();
        exec_code(Code::from_string(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size())));
    }
    std::cerr << "Initial execution done" << std::endl;
}

void ExecutionEngine::subscribe_thunk(std::shared_ptr<const Thunk> source, std::shared_ptr<const Thunk> dest) {
    state.thunk_subscriptions[source].push_back(dest);
}

void ExecutionEngine::finalize_thunk(std::shared_ptr<const Thunk> source, BaseObjectRef result) {
    state.thunk_results[source] = result;
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

TypeRef SubIter::type = create<Type>("SubIter", Type::basevec{Object::type}, Type::attrmap{
    {"__iter__", create<BuiltinFunction>([](ObjectRef self) -> ObjectRef {
        return self;
    })},
    {"__next__", create<BuiltinFunction>([](const SubIter* self) -> BaseObjectRef {
        return self->execengine->make_sub_thunk(self->name, self->position);
    })}
});

SubIter::SubIter(TypeRef type, ExecutionEngine* execengine, DollarName name, unsigned int position) : Object(type), execengine(execengine), name(name), position(position) {
}

SubThunk::SubThunk(ExecutionEngine* execengine, DollarName name, unsigned int position)
    : Thunk(execengine), name(name), position(position) {
}

std::string SubThunk::to_str() const {
    std::stringstream ss;
    ss << "SubT(" << name << " @" << position << ")";
    return ss.str();
}

void SubThunk::handle(std::string name) const {
    auto iter = create<SubIter>(execution_engine(), this->name, position + 1);
    auto obj = create<String>(name);
    finalize(create<List>(std::vector<ObjectRef>{iter, obj}));
}

ModuleThunk::ModuleThunk(ExecutionEngine* execengine, std::string name) : Thunk(execengine), name(name) {
}
