#ifndef BUILTINFUNCTION_HPP
#define BUILTINFUNCTION_HPP

#include "object.hpp"

#include <functional>
#include <stdexcept>
#include <memory>

template<class T> struct convert_from_objref {
    static T convert(const ObjectRef& objref);
};
template<class T> struct convert_to_objref {
    static ObjectRef convert(const T& t);
};

template<class T, class... Args> ObjectRef fw(const std::vector<ObjectRef>& args, std::function<T(Args...)> f) {
    return fw(args, f, std::index_sequence_for<Args...>{});
}

template<class T, class... Args, class I, I... Ints> ObjectRef fw(const std::vector<ObjectRef>& args, std::function<T(Args...)> f, std::integer_sequence<I, Ints...>) {
    if (sizeof...(Args) != args.size()) {
        throw std::runtime_error("Wrong number of args");
    }
    return convert_to_objref<T>::convert(f(convert_from_objref<Args>::convert(args[Ints])...));
}

struct AbstractFunctionHolder {
    virtual ObjectRef call(const std::vector<ObjectRef>& args) = 0;
    virtual ~AbstractFunctionHolder() = default;
};

template<class T, class... Args> struct FunctionHolder : public AbstractFunctionHolder {
    std::function<T(Args...)> f;
    FunctionHolder(std::function<T(Args...)> f) : f(f) {}
    ObjectRef call(const std::vector<ObjectRef>& args) { return fw(args, f); }
};

template<class T> struct FunctionHolder<T, const std::vector<ObjectRef>&> : public AbstractFunctionHolder {
    std::function<T(const std::vector<ObjectRef>&)> f;
    FunctionHolder(std::function<T(const std::vector<ObjectRef>&)> f) : f(f) {}
    ObjectRef call(const std::vector<ObjectRef>& args) { return convert_to_objref<T>::convert(f(args)); }
};

extern std::shared_ptr<Type> builtin_function_type;

class BuiltinFunction : public Object {
    std::unique_ptr<AbstractFunctionHolder> func;
public:
    template<class F> BuiltinFunction(F f) : Object(builtin_function_type), func(new FunctionHolder(std::function(f))) {}
    std::string to_str() override { return "BF"; }
    ObjectRef call(const std::vector<ObjectRef>& args) override { return func->call(args); }
};

// TODO: collapse fw into FunctionHolder, and then collapse FunctionHolder into BuiltinFunction (maybe).

template<> struct convert_from_objref<int> { static int convert(const ObjectRef& objref); };
template<> struct convert_from_objref<unsigned char> { static unsigned char convert(const ObjectRef& objref); };
template<> struct convert_from_objref<double> { static double convert(const ObjectRef& objref); };
template<> struct convert_from_objref<std::string> { static std::string convert(const ObjectRef& objref); };
template<> struct convert_from_objref<std::basic_string<unsigned char>> { static std::basic_string<unsigned char> convert(const ObjectRef& objref); };
template<> struct convert_from_objref<ObjectRef> { static ObjectRef convert(const ObjectRef& objref); };
template<class T> struct convert_from_objref<std::shared_ptr<T>> {
    static std::shared_ptr<T> convert(const ObjectRef& objref) {
        if (auto obj = std::dynamic_pointer_cast<T>(objref)) {
            return obj;
        }
        throw std::runtime_error("Not the right type");
    }
};
template<class T> struct convert_from_objref<T*> {
    static T* convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<T*>(objref.get())) {
            return obj;
        }
        throw std::runtime_error("Not the right type");
    }
};
template<class V> struct convert_from_objref<std::vector<V>> {
    static std::vector<V> convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<List*>(objref.get())) {
            std::vector<V> res;
            for (auto item : obj->get()) {
                res.push_back(convert_from_objref<V>::convert(item));
            }
            return res;
        }
        throw std::runtime_error("Not a list");
    }
};
template<class K, class V> struct convert_from_objref<std::map<K, V>> {
    static std::map<K, V> convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<Dict*>(objref.get())) {
            std::map<K, V> res;
            for (auto item : obj->get()) {
                res[convert_from_objref<K>::convert(item.first)] = convert_from_objref<V>::convert(item.second);
            }
            return res;
        }
        throw std::runtime_error("Not a dict");
    }
};

template<> struct convert_to_objref<int> { static ObjectRef convert(const int& objref); };
template<> struct convert_to_objref<bool> { static ObjectRef convert(const bool& objref); };
template<> struct convert_to_objref<double> { static ObjectRef convert(const double& objref); };
template<> struct convert_to_objref<std::string> { static ObjectRef convert(const std::string& objref); };
template<> struct convert_to_objref<ObjectRef> { static ObjectRef convert(const ObjectRef& objref); };
template<class T> struct convert_to_objref<std::shared_ptr<T>> {
    static ObjectRef convert(const std::shared_ptr<T>& objref) {
        return objref;
    }
};

// Deduction helpers

template<class T, class... Args> std::function<std::shared_ptr<T>(Args...)> constructor() {
    return {std::make_shared<T, Args...>};
}

template<class T, class R, class... Args> std::function<R(T*, Args...)> method(R(T::*meth)(Args...)) {
    return {meth};
}

#endif // BUILTINFUNCTION_HPP
