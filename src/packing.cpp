#include "packing.h"

int roundUp(int n, int multiple) {
    return std::ceil(static_cast<float>(n) / multiple) * multiple;
}

PackingError::PackingError(const std::string& message) : std::runtime_error(message) {}

PackingType::PackingType(int size, int alignment) : size(size), alignment(alignment) {}

i32Type::i32Type() : PackingType(4, 4) {}

int i32Type::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<int32_t>(value)) {
        throw PackingError("Expected int32_t, got " + std::string(typeid(value).name()));
    }
    int32_t val = std::get<int32_t>(value);
    std::memcpy(&buffer[offset], &val, sizeof(int32_t));
    return offset + size;
}

std::pair<int, NestedData> i32Type::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    int32_t content;
    std::memcpy(&content, &buffer[offset], sizeof(int32_t));
    return {offset + size, content};
}

u32Type::u32Type() : PackingType(4, 4) {}

int u32Type::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<uint32_t>(value)) {
        throw PackingError("Expected uint32_t, got " + std::string(typeid(value).name()));
    }
    uint32_t val = std::get<uint32_t>(value);
    std::memcpy(&buffer[offset], &val, sizeof(uint32_t));
    return offset + size;
}

std::pair<int, NestedData> u32Type::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    uint32_t content;
    std::memcpy(&content, &buffer[offset], sizeof(uint32_t));
    return {offset + size, content};
}

f32Type::f32Type() : PackingType(4, 4) {}

int f32Type::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<float>(value)) {
        throw PackingError("Expected float, got " + std::string(typeid(value).name()));
    }
    float val = std::get<float>(value);
    std::memcpy(&buffer[offset], &val, sizeof(float));
    return offset + size;
}

std::pair<int, NestedData> f32Type::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    float content;
    std::memcpy(&content, &buffer[offset], sizeof(float));
    return {offset + size, content};
}

const i32Type i32;
const u32Type u32;
const f32Type f32;

VectorType::VectorType(const PackingType& baseType, int nValues, int alignment)
    : PackingType(baseType.size * nValues, alignment), baseType(baseType), nValues(nValues) {}

int VectorType::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<std::vector<NestedData>>(value)) {
        throw PackingError("Expected array, got " + std::string(typeid(value).name()));
    }

    const auto& values = std::get<std::vector<NestedData>>(value);

    if (values.size() != nValues) {
        throw PackingError("Expected " + std::to_string(nValues) + " values, got " + std::to_string(values.size()));
    }

    while (offset % alignment != 0) {
        offset++;
    }

    for (int i = 0; i < values.size(); ++i) {
        try {
            offset = baseType.pack(offset, values[i], buffer);
        } catch (const PackingError& e) {
            throw PackingError("Error packing value " + std::to_string(i) + ": " + e.what());
        }
    }
    return offset;
}

std::pair<int, NestedData> VectorType::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    std::vector<NestedData> values;

    while (offset % alignment != 0) {
        offset++;
    }

    for (int i = 0; i < nValues; ++i) {
        auto [newOffset, value] = baseType.unpack(offset, buffer);
        offset = newOffset;
        values.push_back(value);
    }
    return {offset, values};
}

vec2::vec2(const PackingType& baseType) : VectorType(baseType, 2, 8) {}

vec3::vec3(const PackingType& baseType) : VectorType(baseType, 3, 16) {}

vec4::vec4(const PackingType& baseType) : VectorType(baseType, 4, 16) {}

Struct::Struct(const std::vector<std::pair<std::string, PackingType*>>& members)
    : PackingType(0, 0), members(members) {
    int alignment = 0;
    int offset = 0;
    for (const auto& member : members) {
        alignment = std::max(alignment, member.second->alignment);
        while (offset % member.second->alignment != 0) {
            offset++;
        }
        offset += member.second->size;
    }
    size = roundUp(offset, alignment);
    this->alignment = alignment;
}

int Struct::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<std::unordered_map<std::string, NestedData>>(value)) {
        throw PackingError("Expected object, got " + std::string(typeid(value).name()));
    }

    const auto& values = std::get<std::unordered_map<std::string, NestedData>>(value);

    if (values.size() != members.size()) {
        throw PackingError("Expected " + std::to_string(members.size()) + " values, got " + std::to_string(values.size()));
    }

    const int startingOffset = offset;

    while (offset % alignment != 0) {
        offset++;
    }

    for (const auto& member : members) {
        const auto& key = member.first;
        const auto& type = member.second;
        if (values.find(key) == values.end()) {
            throw PackingError("Missing value for key " + key);
        }
        const auto& memberValue = values.at(key);
        try {
            offset = type->pack(offset, memberValue, buffer);
        } catch (const PackingError& e) {
            throw PackingError("Error packing value " + key + ": " + e.what());
        }
    }

    offset += size - (offset - startingOffset);
    return offset;
}

std::pair<int, NestedData> Struct::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    std::unordered_map<std::string, NestedData> values;
    const int startingOffset = offset;

    while (offset % alignment != 0) {
        offset++;
    }

    for (const auto& member : members) {
        const auto& key = member.first;
        const auto& type = member.second;
        auto [newOffset, value] = type->unpack(offset, buffer);
        offset = newOffset;
        values[key] = value;
    }

    offset += size - (offset - startingOffset);
    return {offset, values};
}

StaticArray::StaticArray(const PackingType& type, int nElements)
    : PackingType(nElements * roundUp(type.size, type.alignment), type.alignment),
      type(type), nElements(nElements),
      stride(roundUp(type.size, type.alignment)) {}

int StaticArray::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<std::vector<NestedData>>(value)) {
        throw PackingError("Expected array, got " + std::string(typeid(value).name()));
    }

    const auto& values = std::get<std::vector<NestedData>>(value);

    if (values.size() != nElements) {
        throw PackingError("Expected " + std::to_string(nElements) + " values, got " + std::to_string(values.size()));
    }

    while (offset % alignment != 0) {
        offset++;
    }

    for (int i = 0; i < values.size(); ++i) {
        try {
            offset = type.pack(offset, values[i], buffer);
        } catch (const PackingError& e) {
            throw PackingError("Error packing value " + std::to_string(i) + ": " + e.what());
        }
        offset += stride - type.size;
    }
    return offset;
}

std::pair<int, NestedData> StaticArray::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    std::vector<NestedData> values;

    while (offset % alignment != 0) {
        offset++;
    }

    for (int i = 0; i < nElements; ++i) {
        auto [newOffset, value] = type.unpack(offset, buffer);
        offset = newOffset;
        values.push_back(value);
        offset += stride - type.size;
    }
    return {offset, values};
}

MatrixType::MatrixType(const PackingType& baseType, int nRows, int nColumns)
    : PackingType(0, 0), baseType(baseType), nRows(nRows), nColumns(nColumns) {
    VectorType* vecType;
    if (nRows == 2) {
        vecType = new vec2(baseType);
    } else if (nRows == 3) {
        vecType = new vec3(baseType);
    } else if (nRows == 4) {
        vecType = new vec4(baseType);
    } else {
        throw std::invalid_argument("Invalid number of rows: " + std::to_string(nRows));
    }
    StaticArray arrayType(*vecType, nColumns);
    size = arrayType.size;
    alignment = vecType->alignment;
}

int MatrixType::pack(int offset, const NestedData& value, std::vector<uint8_t>& buffer) const {
    if (!std::holds_alternative<std::vector<NestedData>>(value)) {
        throw PackingError("Expected array, got " + std::string(typeid(value).name()));
    }

    const auto& values = std::get<std::vector<NestedData>>(value);

    if (values.size() != nColumns) {
        throw PackingError("Expected " + std::to_string(nColumns) + " columns, got " + std::to_string(values.size()));
    }

    while (offset % alignment != 0) {
        offset++;
    }

    const int startOffset = offset;

    for (int i = 0; i < values.size(); ++i) {
        if (!std::holds_alternative<std::vector<NestedData>>(values[i])) {
            throw PackingError("Expected array, got " + std::string(typeid(values[i]).name()));
        }

        const auto& innerValues = std::get<std::vector<NestedData>>(values[i]);

        for (int j = 0; j < innerValues.size(); ++j) {
            try {
                offset = baseType.pack(offset, innerValues[j], buffer);
            } catch (const PackingError& e) {
                throw PackingError("Error packing value " + std::to_string(i) + "," + std::to_string(j) + ": " + e.what());
            }
        }
    }

    offset = startOffset + size;
    return offset;
}

std::pair<int, NestedData> MatrixType::unpack(int offset, const std::vector<uint8_t>& buffer) const {
    while (offset % alignment != 0) {
        offset++;
    }

    const int startOffset = offset;
    std::vector<NestedData> outerValues;

    for (int i = 0; i < nColumns; ++i) {
        std::vector<NestedData> innerValues;
        for (int j = 0; j < nRows; ++j) {
            auto [newOffset, value] = baseType.unpack(offset, buffer);
            offset = newOffset;
            innerValues.push_back(value);
        }
        outerValues.push_back(innerValues);
    }

    offset += size - (offset - startOffset);
    return {offset, outerValues};
}

mat4x4::mat4x4(const PackingType& baseType) : MatrixType(baseType, 4, 4) {}
