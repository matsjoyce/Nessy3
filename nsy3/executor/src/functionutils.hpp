#ifndef FUNCTIONUTILS_HPP
#define FUNCTIONUTILS_HPP

#include <functional>
#include <stdexcept>
#include <memory>

#include "object.hpp"
#include "exception.hpp"

// TODO: collapse fw into FunctionHolder, and then collapse FunctionHolder into BuiltinFunction (maybe).

template<> struct convert_from_objref<int> { static int convert(const ObjectRef& objref); };
template<> struct convert_from_objref<unsigned char> { static unsigned char convert(const ObjectRef& objref); };
template<> struct convert_from_objref<unsigned int> { static unsigned int convert(const ObjectRef& objref); };
template<> struct convert_from_objref<double> { static double convert(const ObjectRef& objref); };
template<> struct convert_from_objref<bool> { static bool convert(const ObjectRef& objref); };
template<> struct convert_from_objref<std::string> { static std::string convert(const ObjectRef& objref); };
template<> struct convert_from_objref<std::basic_string<unsigned char>> { static std::basic_string<unsigned char> convert(const ObjectRef& objref); };
template<> struct convert_from_objref<ObjectRef> { static ObjectRef convert(const ObjectRef& objref); };
template<class T> struct convert_from_objref<std::shared_ptr<const T>> {
    static std::shared_ptr<const T> convert(const ObjectRef& objref) {
        if (auto obj = std::dynamic_pointer_cast<const T>(objref)) {
            return obj;
        }
        create<TypeError>("Expected a " + T::type->name() + ", got " + objref->obj_type()->name())->raise();
    }
};
template<class T> struct convert_from_objref<const T*> {
    static const T* convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<const T*>(objref.get())) {
            return obj;
        }
        create<TypeError>("Expected a " + T::type->name() + ", got " + objref->obj_type()->name())->raise();
    }
};
template<class V> struct convert_from_objref<std::vector<V>> {
    static std::vector<V> convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<const List*>(objref.get())) {
            std::vector<V> res;
            for (auto item : obj->get()) {
                res.push_back(convert_from_objref<V>::convert(item));
            }
            return res;
        }
        create<TypeError>("Expected a list, got " + objref->obj_type()->name())->raise();
    }
};
template<class K, class V> struct convert_from_objref<std::map<K, V>> {
    static std::map<K, V> convert(const ObjectRef& objref) {
        if (auto obj = dynamic_cast<const Dict*>(objref.get())) {
            std::map<K, V> res;
            for (auto item : obj->get()) {
                res[convert_from_objref<K>::convert(item.first)] = convert_from_objref<V>::convert(item.second);
            }
            return res;
        }
        create<TypeError>("Expected a dict, got " + objref->obj_type()->name())->raise();
    }
};

template<> struct convert_to_objref<int> { static BaseObjectRef convert(const int& objref); };
template<> struct convert_to_objref<bool> { static BaseObjectRef convert(const bool& objref); };
template<> struct convert_to_objref<double> { static BaseObjectRef convert(const double& objref); };
template<> struct convert_to_objref<std::string> { static BaseObjectRef convert(const std::string& objref); };
template<class T> struct convert_to_objref<std::shared_ptr<const T>> {
    static BaseObjectRef convert(const std::shared_ptr<const T>& objref) {
        return objref;
    }
};

// Deduction helpers

template<class T, class... Args> std::function<std::shared_ptr<const T>(Args...)> constructor() {
    return {create<T, Args...>};
}

template<class T, class R, class... Args> std::function<R(const T*, Args...)> method(R(T::*meth)(Args...) const) {
    return {meth};
}

template<class T, class R, class... Args> std::function<R(Args...)> method_and_bind(T* t, R(T::*meth)(Args...)) {
    return [=](Args... args){ return (t->*meth)(args...); };
}

template<class T, class R, class... Args> std::function<R(Args...)> method_and_bind(const T* t, R(T::*meth)(Args...) const) {
    return [=](Args... args){ return (t->*meth)(t, args...); };
}

// Utils

template<class T> T convert(const ObjectRef& obj) {
    return convert_from_objref<T>::convert(obj);
}

template<class T> std::shared_ptr<const T> convert_ptr(const ObjectRef& obj) {
    return convert_from_objref<std::shared_ptr<const T>>::convert(obj);
}

#endif // FUNCTIONUTILS_HPP
