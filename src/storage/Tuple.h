#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include "../catalog/Type.h"

// Serializes/deserializes a row (vector of Values) to/from raw bytes
class Tuple {
public:
    // Serialize values ? bytes
    static std::vector<char> serialize(const std::vector<Value> &values);

    // Deserialize bytes ? values (needs column types to know how to read)
    static std::vector<Value> deserialize(const char *data, uint16_t len,
                                          const std::vector<DataType> &types);
};
