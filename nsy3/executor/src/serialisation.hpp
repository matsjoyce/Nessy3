#ifndef SERIALISATION_HPP
#define SERIALISATION_HPP

#include <string>
#include <utility>

#include "object.hpp"

std::pair<ObjectRef, unsigned int> deserialise(std::basic_string<unsigned char> bytes, unsigned int pos);

#endif // SERIALISATION_HPP
