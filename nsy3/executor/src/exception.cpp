#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

#include "exception.hpp"
#include "bytecode.hpp"

TypeRef Exception::type = create<Type>("Exception", Type::basevec{Object::type});

Exception::Exception(TypeRef type, ObjectRef reason, std::vector< std::pair< std::string, int > > stack_trace) : Object(type), reason_(reason), stack_trace_(stack_trace) {
}

std::string Exception::to_str() const {
    std::stringstream ss;
    ss << "Traceback (most recent call last):\n";
    for (auto& [fname, lineno] : stack_trace_) {
        ss << "  File \"" << fname << "\", line " << lineno << "\n";
        ss << "    " << get_line_of_file(fname, lineno, true) << "\n";
    }
    ss << reason_->to_str();
    return ss.str();
}

std::shared_ptr<const Exception> Exception::append_stack(std::string fname, int lineno) const {
    auto copy = stack_trace_;
    copy.emplace_back(fname, lineno);
    return create<Exception>(reason_, copy);
}


[[ noreturn ]] void Exception::raise() const {
    throw ExceptionContainer(std::dynamic_pointer_cast<const Exception>(self()));
}


ExceptionContainer::ExceptionContainer(std::shared_ptr<const Exception> exception) : exception(exception) {
}

const char* ExceptionContainer::what() const noexcept {
    return "ExceptionContainer (catch explicityly to get the deetz)";
}

TypeRef Error::type = create<Type>("Error", Type::basevec{Object::type});

Error::Error(TypeRef type, ObjectRef details) : Object(type), details_(details) {
}

Error::Error(TypeRef type, std::string details) : Error(type, create<String>(details)) {
}

[[ noreturn ]] void Error::raise() const {
    create<Exception>(self())->raise();
}

std::string Error::to_str() const {
    std::stringstream ss;
    ss << obj_type()->name() << ": " << details_->to_str();
    return ss.str();
}

TypeRef TypeError::type = create<Type>("TypeError", Type::basevec{Error::type});
TypeRef UnsupportedOperation::type = create<Type>("UnsupportedOperation", Type::basevec{TypeError::type});
TypeRef NameError::type = create<Type>("NameError", Type::basevec{Error::type});
TypeRef IndexError::type = create<Type>("IndexError", Type::basevec{Error::type});
TypeRef AssertionError::type = create<Type>("AssertionError", Type::basevec{Error::type});
TypeRef ValueError::type = create<Type>("ValueError", Type::basevec{Error::type});
