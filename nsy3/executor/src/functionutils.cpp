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

unsigned int convert_from_objref<unsigned int>::convert(const ObjectRef& objref) {
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

bool convert_from_objref<bool>::convert(const ObjectRef& objref) {
    return objref->to_bool();
}

std::string convert_from_objref<std::string>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const String*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not a string");
}

std::basic_string<unsigned char> convert_from_objref<std::basic_string<unsigned char>>::convert(const ObjectRef& objref) {
    if (auto obj = dynamic_cast<const Bytes*>(objref.get())) {
        return obj->get();
    }
    throw std::runtime_error("Not bytes");
}

ObjectRef convert_from_objref<ObjectRef>::convert(const ObjectRef& objref) {
    return objref;
}

BaseObjectRef convert_to_objref<int>::convert(const int& t) {
    return create<Integer>(t);
}

BaseObjectRef convert_to_objref<bool>::convert(const bool& t) {
    return t ? Boolean::true_ : Boolean::false_;
}

BaseObjectRef convert_to_objref<double>::convert(const double& t) {
    return create<Float>(t);
}

BaseObjectRef convert_to_objref<std::string>::convert(const std::string& t) {
    return create<String>(t);
}
