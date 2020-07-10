#include "exception.hpp"

TypeRef Exception::type = create<Type>("Exception");

Exception::Exception(TypeRef type, std::string reason) : Object(type), reason(reason) {
}

std::string Exception::to_str() const {
    return obj_type()->name() + ": " + reason;
}

void Exception::raise() const {
    throw shared_from_this();
}

TypeRef TypeException::type = create<Type>("TypeException");
TypeRef NameException::type = create<Type>("NameException");
TypeRef AssertionException::type = create<Type>("AssertionException");
