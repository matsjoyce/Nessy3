#include "serialisation.hpp"

#include <stdexcept>
#include <istream>
#include <iostream>

enum class SerialisationType : char {
    INT, FLOAT, STRING, DICT, SET, LIST, BYTES, TRUE, FALSE, NONE
};

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
        case SerialisationType::TRUE: {
            return Boolean::true_;
        }
        case SerialisationType::FALSE: {
            return Boolean::false_;
        }
        case SerialisationType::NONE: {
            return NoneType::none;
        }
        default: {
            throw std::runtime_error("Unknown serialisation type" + std::to_string(static_cast<int>(type)));
        }
    }
}

void serialize_to_file(std::ostream& stream, ObjectRef obj) {
    if (auto ptr = dynamic_cast<const Integer*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::INT);
        auto v = ptr->get();
        stream.write(reinterpret_cast<const char*>(&v), 4);
    }
    else if (auto ptr = dynamic_cast<const Float*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::FLOAT);
        auto v = ptr->get();
        stream.write(reinterpret_cast<const char*>(&v), 8);
    }
    else if (auto ptr = dynamic_cast<const String*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::STRING);
        auto v = ptr->get().size();
        stream.write(reinterpret_cast<const char*>(&v), 4);
        stream << ptr->get();
    }
    else if (auto ptr = dynamic_cast<const Bytes*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::BYTES);
        auto v = ptr->get().size();
        stream.write(reinterpret_cast<const char*>(&v), 4);
        stream.write(reinterpret_cast<const char*>(ptr->get().data()), v);
    }
    else if (auto ptr = dynamic_cast<const List*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::LIST);
        auto v = ptr->get().size();
        stream.write(reinterpret_cast<const char*>(&v), 4);
        for (auto& item : ptr->get()) {
            serialize_to_file(stream, item);
        }
    }
    else if (auto ptr = dynamic_cast<const Dict*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::DICT);
        auto v = ptr->get().size();
        stream.write(reinterpret_cast<const char*>(&v), 4);
        for (auto& item : ptr->get()) {
            serialize_to_file(stream, item.first);
            serialize_to_file(stream, item.second);
        }
    }
    else if (auto ptr = dynamic_cast<const NoneType*>(obj.get())) {
        stream << static_cast<char>(SerialisationType::NONE);
    }
    else if (auto ptr = dynamic_cast<const Boolean*>(obj.get())) {
        stream << static_cast<char>(ptr->get() ? SerialisationType::TRUE : SerialisationType::FALSE);
    }
    else {
        throw std::runtime_error("Unknown serialisation type");
    }
}
