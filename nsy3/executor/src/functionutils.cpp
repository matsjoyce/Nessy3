#include "functionutils.hpp"

int convert_from_objref<int>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const Integer*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not an int");
}

unsigned char convert_from_objref<unsigned char>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const Integer*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not an int");
}

double convert_from_objref<double>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const Numeric*>(objref.get())) {
        return obj->to_double();
    }
    throw std::runtime_error("Not numeric");
}

std::string convert_from_objref<std::string>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const String*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not numeric");
}

std::basic_string<unsigned char> convert_from_objref<std::basic_string<unsigned char>>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const Bytes*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not numeric");
}

ObjectRef convert_from_objref<ObjectRef>::convert(const ObjectRef& objref) {
    return objref;
}

ObjectRef convert_to_objref<int>::convert(const int& t) {
    return create<Integer>(t);
}

ObjectRef convert_to_objref<bool>::convert(const bool& t) {
    return create<Boolean>(t);
}

ObjectRef convert_to_objref<double>::convert(const double& t) {
    return create<Float>(t);
}

ObjectRef convert_to_objref<std::string>::convert(const std::string& t) {
    return create<String>(t);
}

ObjectRef convert_to_objref<ObjectRef>::convert(const ObjectRef& t) {
    return t;
}
