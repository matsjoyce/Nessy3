#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include "object.hpp"

class Exception : public Object {
    std::string reason;
public:
    Exception(TypeRef type, std::string reason);
    static TypeRef type;
    std::string to_str() const override;
    void raise() const;
};

class TypeException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

class UnsupportedOperation : public TypeException {
public:
    using TypeException::TypeException;
    static TypeRef type;
};

class NameException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

class IndexException : public Exception {
public:
    using Exception::Exception;
    static TypeRef type;
};

class ValueException : public Exception {
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
