#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include "object.hpp"

class Exception : public Object {
    ObjectRef reason_;
    std::vector<std::pair<std::string, int>> stack_trace_;
public:
    Exception(TypeRef type, ObjectRef reason, std::vector<std::pair<std::string, int>> stack_trace = {});
    static TypeRef type;
    std::string to_str() const override;
    [[ noreturn ]] void raise() const;
    std::shared_ptr<const Exception> append_stack(std::string fname, int lineno) const;
    ObjectRef reason() const { return reason_; };
};

class ExceptionContainer : public std::exception {
public:
    ExceptionContainer(std::shared_ptr<const Exception> exception);
    std::shared_ptr<const Exception> exception;
    const char* what() const noexcept override;
};


class Error : public Object {
    ObjectRef details_;
public:
    Error(TypeRef type, ObjectRef details);
    Error(TypeRef type, std::string details);
    static TypeRef type;
    std::string to_str() const override;
    [[ noreturn ]] void raise() const;
};

class TypeError : public Error {
public:
    using Error::Error;
    static TypeRef type;
};

class UnsupportedOperation : public TypeError {
public:
    using TypeError::TypeError;
    static TypeRef type;
};

class NameError : public Error {
public:
    using Error::Error;
    static TypeRef type;
};

class IndexError : public Error {
public:
    using Error::Error;
    static TypeRef type;
};

class ValueError : public Error {
public:
    using Error::Error;
    static TypeRef type;
};

class AssertionError : public Error {
public:
    using Error::Error;
    static TypeRef type;
};

#endif // EXCEPTION_HPP
