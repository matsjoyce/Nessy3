#include "serialisation.hpp"

#include <stdexcept>

enum class SerialisationType : char {
    INT, FLOAT, STRING, DICT, SET, LIST
};

std::pair<ObjectRef, unsigned int> deserialise(std::basic_string<unsigned char> bytes, unsigned int pos) {
    auto type = static_cast<SerialisationType>(bytes[pos]);
    switch (type) {
        case SerialisationType::INT: {
            return {std::make_shared<Integer>(*reinterpret_cast<int*>(bytes.data() + pos + 1)), pos + 5};
        }
        case SerialisationType::FLOAT: {
            return {std::make_shared<Float>(*reinterpret_cast<double*>(bytes.data() + pos + 1)), pos + 9};
        }
        case SerialisationType::STRING: {
            auto len = *reinterpret_cast<unsigned int*>(bytes.data() + pos + 1);
            auto str = std::string(reinterpret_cast<char*>(bytes.data() + pos + 5), len);
            return {std::make_shared<String>(str), pos + 5 + len};
        }
        case SerialisationType::LIST: {
            auto len = *reinterpret_cast<unsigned int*>(bytes.data() + pos + 1);
            std::vector<ObjectRef> objs;
            pos += 5;
            for (auto i = 0u; i < len; ++i) {
                auto [obj, new_pos] = deserialise(bytes, pos);
                objs.push_back(obj);
                pos = new_pos;
            }
            return {std::make_shared<List>(objs), pos};
        }
        default: {
            throw std::runtime_error("Unknown serialisation type");
        }
    }
}
