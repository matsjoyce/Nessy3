#ifndef SERIALISATION_HPP
#define SERIALISATION_HPP

#include <string>
#include <utility>

#include "object.hpp"

ObjectRef deserialise_from_file(std::istream& stream);
void serialize_to_file(std::ostream& stream, ObjectRef obj);

#endif // SERIALISATION_HPP
