#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include "object.hpp"

class Exception : public Object {
    std::string reason;
public:
    Exception(TypeRef type, std::string reason);
    static TypeRef type;
    std::string to_str() override;
    void raise();
};

class TypeException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

class NameException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

class AssertionException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

#endif // EXCEPTION_HPP
