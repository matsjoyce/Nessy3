#ifndef THUNK_HPP
#define THUNK_HPP

#include <memory>
#include <variant>
#include "object.hpp"


class ExecutionEngine;

class Thunk : public BaseObject {
    ExecutionEngine* execengine;
    bool finalized = false;
public:
    Thunk(ExecutionEngine* execengine);
    virtual ~Thunk();
    void subscribe(std::shared_ptr<const Thunk> thunk) const;
    virtual void notify(BaseObjectRef obj) const;
    void finalize(BaseObjectRef obj) const;
    virtual std::string to_str() const;
    ExecutionEngine* execution_engine() const { return execengine; }
};

#endif // THUNK_HPP
