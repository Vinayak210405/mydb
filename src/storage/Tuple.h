#pragma once
#include <vector>
#include <cstdint>
#include "../catalog/Type.h"

// ---------------------------------------------------------------------------
// Tuple — turns a row (vector of Values) into bytes and back.
//
// Per field: 1-byte type tag, then the payload
//   0 NULL   : nothing
//   1 INT    : 8 bytes (int64)
//   2 FLOAT  : 8 bytes (double)
//   3 STRING : 4-byte length + bytes
//   4 BOOL   : 1 byte
// ---------------------------------------------------------------------------
class Tuple {
public:
    static std::vector<char>  serialize(const std::vector<Value> &values);
    // Throws std::runtime_error if the bytes are truncated or malformed.
    static std::vector<Value> deserialize(const char *data, size_t len, size_t num_columns);
};
