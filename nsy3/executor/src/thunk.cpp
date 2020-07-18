#include "thunk.hpp"
#include "executionengine.hpp"

#include <iostream>

Thunk::Thunk(ExecutionEngine* execengine) : execengine(execengine) {
}

Thunk::~Thunk() {
    if (!finalized) {
        std::cerr << "Thunk " << this << " was not finalized!" << std::endl;
    }
}

void Thunk::subscribe(std::shared_ptr<const Thunk> thunk) const {
    execengine->subscribe_thunk(std::dynamic_pointer_cast<const Thunk>(shared_from_this()), thunk);
}

void Thunk::notify(BaseObjectRef /*obj*/) const {
}

void Thunk::finalize(BaseObjectRef obj) const {
    const_cast<Thunk*>(this)->finalized = true;
    execengine->finalize_thunk(std::dynamic_pointer_cast<const Thunk>(shared_from_this()), std::move(obj));
}

std::string Thunk::to_str() const {
    return "T(?)";
}
