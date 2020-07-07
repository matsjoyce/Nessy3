#ifndef FRAME_HPP
#define FRAME_HPP

#include <map>
#include <vector>

#include "bytecode.hpp"
#include "object.hpp"

class Frame {
    std::shared_ptr<Code> code;
    unsigned int position = 0;
    std::map<std::string, ObjectRef> env;
    std::vector<std::pair<unsigned char, ObjectRef>> stack;
public:
    Frame(std::shared_ptr<Code> code, int offset=0, std::map<std::string, ObjectRef> env={});

    ObjectRef execute();
    void set_env(std::string name, ObjectRef value);
    ObjectRef get_env(std::string name);
};

#endif // FRAME_HPP
