#ifndef EXECUTIONENGINE_PRIVATE_HPP
#define EXECUTIONENGINE_PRIVATE_HPP

#include "executionengine.hpp"

enum class DollarGetFlags : unsigned int {
    PARTIAL = 1 // Can read undecided values
};

enum class DollarSetFlags : unsigned int {
    MODIFICATION = 1, // Can set after the initial set
    DEFAULT = 2 // Only executed if there is no initial set
};

class TestThunk : public Thunk {
    std::string name;
public:
    TestThunk(ExecutionEngine* execengine, std::string name);
    std::string to_str() const override;
};

class GetThunk : public Thunk {
    DollarName name;
    unsigned int flags;
public:
    GetThunk(ExecutionEngine* execengine, DollarName name, unsigned int flags);
    std::string to_str() const override;

    friend class ExecutionEngine;
};

class SetThunk : public Thunk {
    DollarName name;
    ObjectRef value;
    unsigned int flags;
public:
    SetThunk(ExecutionEngine* execengine, DollarName name, ObjectRef value, unsigned int flags);
    std::string to_str() const override;

    friend class ExecutionEngine;
};

class SubIter : public Object {
    ExecutionEngine* execengine;
    DollarName name;
    unsigned int position;
public:
    SubIter(TypeRef type, ExecutionEngine* execengine, DollarName name, unsigned int position=0);
    static TypeRef type;
};

class SubThunk : public Thunk {
    DollarName name;
    unsigned int position;
public:
    SubThunk(ExecutionEngine* execengine, DollarName name, unsigned int position);
    std::string to_str() const override;
    void handle(std::string sub) const;

    friend class ExecutionEngine;
};

class ModuleThunk : public Thunk {
    std::string name;
public:
    ModuleThunk(ExecutionEngine* execengine, std::string name);
};

#endif // EXECUTIONENGINE_PRIVATE_HPP
