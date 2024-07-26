#ifndef PACKING_H
#define PACKING_H

#include <iostream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <variant>
#include <typeinfo>
#include <cstring>
#include <cmath>
#include <string>

// Define NestedData as a variant of possible types
using NestedData = std::variant<int32_t, uint32_t, float, std::vector<NestedData>, std::unordered_map<std::string, NestedData>>;

int roundUp(int n, int multiple);

class PackingError : public std::runtime_error {
public:
    explicit PackingError(const std::string& message);
};

class PackingType {
public:
    int size;
    int alignment;

    PackingType(int size, int alignment);
    virtual int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const = 0;
    virtual std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const = 0;
};

class i32Type : public PackingType {
public:
    i32Type();
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class u32Type : public PackingType {
public:
    u32Type();
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class f32Type : public PackingType {
public:
    f32Type();
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

extern const i32Type i32;
extern const u32Type u32;
extern const f32Type f32;

class VectorType : public PackingType {
public:
    const PackingType& baseType;
    int nValues;

    VectorType(const PackingType& baseType, int nValues, int alignment);
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class vec2 : public VectorType {
public:
    vec2(const PackingType& baseType);
};

class vec3 : public VectorType {
public:
    vec3(const PackingType& baseType);
};

class vec4 : public VectorType {
public:
    vec4(const PackingType& baseType);
};

class Struct : public PackingType {
public:
    std::vector<std::pair<std::string, PackingType*>> members;

    Struct(const std::vector<std::pair<std::string, PackingType*>>& members);
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class StaticArray : public PackingType {
public:
    const PackingType& type;
    int nElements;
    int stride;

    StaticArray(const PackingType& type, int nElements);
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class MatrixType : public PackingType {
public:
    const PackingType& baseType;
    int nRows;
    int nColumns;

    MatrixType(const PackingType& baseType, int nRows, int nColumns);
    int pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const override;
    std::pair<int, NestedData> unpack(int offset, const std::vector<uint8_t>& buffer) const override;
};

class mat4x4 : public MatrixType {
public:
    mat4x4(const PackingType& baseType);
};

#endif // PACKING_H
