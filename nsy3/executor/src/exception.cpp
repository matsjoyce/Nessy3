#include "exception.hpp"

TypeRef Exception::type = create<Type>("Exception", Type::basevec{Object::type});

Exception::Exception(TypeRef type, std::string reason) : Object(type), reason(reason) {
}

std::string Exception::to_str() const {
    return obj_type()->name() + ": " + reason;
}

void Exception::raise() const {
    throw shared_from_this();
}

TypeRef TypeException::type = create<Type>("TypeException", Type::basevec{Exception::type});
TypeRef NameException::type = create<Type>("NameException", Type::basevec{Exception::type});
TypeRef AssertionException::type = create<Type>("AssertionException", Type::basevec{Exception::type});
