#include "serialisation.hpp"

#include <stdexcept>
#include <istream>
#include <iostream>

enum class SerialisationType : char {
    INT, FLOAT, STRING, DICT, SET, LIST, BYTES, TRUE, FALSE, NONE
};

std::pair<ObjectRef, unsigned int> deserialise(std::basic_string<unsigned char> bytes, unsigned int pos) {
    auto type = static_cast<SerialisationType>(bytes[pos]);
    switch (type) {
        case SerialisationType::INT: {
            return {create<Integer>(*reinterpret_cast<int*>(bytes.data() + pos + 1)), pos + 5};
        }
        case SerialisationType::FLOAT: {
            return {create<Float>(*reinterpret_cast<double*>(bytes.data() + pos + 1)), pos + 9};
        }
        case SerialisationType::STRING: {
            auto len = *reinterpret_cast<unsigned int*>(bytes.data() + pos + 1);
            auto str = std::string(reinterpret_cast<char*>(bytes.data() + pos + 5), len);
            return {create<String>(str), pos + 5 + len};
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
            return {create<List>(objs), pos};
        }
        case SerialisationType::DICT: {
            auto len = *reinterpret_cast<unsigned int*>(bytes.data() + pos + 1);
            ObjectRefMap objs;
            pos += 5;
            for (auto i = 0u; i < len; ++i) {
                auto [key, new_pos] = deserialise(bytes, pos);
                auto [value, new_pos2] = deserialise(bytes, new_pos);
                objs[key] = value;
                pos = new_pos2;
            }
            return {create<Dict>(objs), pos};
        }
        case SerialisationType::BYTES: {
            auto len = *reinterpret_cast<unsigned int*>(bytes.data() + pos + 1);
            return {create<Bytes>(bytes.substr(pos + 5, len)), pos + 5 + len};
        }
        default: {
            throw std::runtime_error("Unknown serialisation type");
        }
    }
}

ObjectRef deserialise_from_file(std::istream& stream) {
    auto type = static_cast<SerialisationType>(stream.get());
    switch (type) {
        case SerialisationType::INT: {
            char bytes[4];
            stream.read(bytes, 4);
            return create<Integer>(*reinterpret_cast<int*>(bytes));
        }
        case SerialisationType::FLOAT: {
            char bytes[8];
            stream.read(bytes, 8);
            return create<Float>(*reinterpret_cast<double*>(bytes));
        }
        case SerialisationType::STRING: {
            char bytes[4];
            stream.read(bytes, 4);
            auto len = *reinterpret_cast<unsigned int*>(bytes);
            auto str = std::string(len, '\0');
            stream.read(str.data(), len);
            return create<String>(str);
        }
        case SerialisationType::LIST: {
            char bytes[4];
            stream.read(bytes, 4);
            auto len = *reinterpret_cast<unsigned int*>(bytes);
            std::vector<ObjectRef> objs;
            for (auto i = 0u; i < len; ++i) {
                objs.push_back(deserialise_from_file(stream));
            }
            return create<List>(objs);
        }
        case SerialisationType::DICT: {
            char bytes[4];
            stream.read(bytes, 4);
            auto len = *reinterpret_cast<unsigned int*>(bytes);
            ObjectRefMap objs;
            for (auto i = 0u; i < len; ++i) {
                auto key = deserialise_from_file(stream);
                auto value = deserialise_from_file(stream);
                objs[key] = value;
            }
            return create<Dict>(objs);
        }
        case SerialisationType::BYTES: {
            char bytes[4];
            stream.read(bytes, 4);
            auto len = *reinterpret_cast<unsigned int*>(bytes);
            auto str = std::basic_string<unsigned char>(len, '\0');
            stream.read(reinterpret_cast<char*>(str.data()), len);
            return create<Bytes>(str);
        }
        default: {
            throw std::runtime_error("Unknown serialisation type" + std::to_string(static_cast<int>(type)));
        }
    }
}
