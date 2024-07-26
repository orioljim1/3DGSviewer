#include "ply.h"
#include <fstream>
#include <iostream>
#include <cmath>

std::tuple<int, std::map<std::string, std::string>, std::span<const uint8_t>, std::map<int, std::string>> PackedGaussians::decodeHeader(std::span<const uint8_t> plyArrayBuffer) {
    std::string headerText;
    size_t headerOffset = 0;

    while (headerOffset < plyArrayBuffer.size()) {
        headerText += std::string(plyArrayBuffer.begin() + headerOffset, plyArrayBuffer.begin() + headerOffset + 50);
        headerOffset += 50;

        if (headerText.find("end_header") != std::string::npos) {
            break;
        }
    }

    size_t header_end = headerText.find("end_header");
    std::vector<std::string> headerLines;
    size_t pos = 0;
    while ((pos = headerText.find('\n')) != std::string::npos) {
        headerLines.push_back(headerText.substr(0, pos));
        headerText.erase(0, pos + 1);
    }

    int vertexCount = 0;
    std::map<std::string, std::string> propertyTypes;
    std::map<int, std::string> intKeys; // To store integer-like keys
    int key = 0;

    for (const auto& line : headerLines) {
        std::string trimmedLine = line;
        trimmedLine.erase(trimmedLine.find_last_not_of(" \n\r\t") + 1);
        if (trimmedLine.find("element vertex") == 0) {
            vertexCount = std::stoi(trimmedLine.substr(15));
        }
        else if (trimmedLine.find("property") == 0) {
            size_t firstSpace = trimmedLine.find(' ');
            size_t secondSpace = trimmedLine.find(' ', firstSpace + 1);
            std::string propertyType = trimmedLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            std::string propertyName = trimmedLine.substr(secondSpace + 1);
            propertyTypes[propertyName] = propertyType;
            intKeys[key] = propertyName;
            key++;
        }
        else if (trimmedLine == "end_header") {
            break;
        }
    }

    size_t vertexByteOffset = header_end + std::string("end_header").length() + 1;
    auto vertexData = std::span<const uint8_t>(plyArrayBuffer.begin() + vertexByteOffset, plyArrayBuffer.end());

    return { vertexCount, propertyTypes, vertexData, intKeys };
}

std::tuple<size_t, std::map<std::string, float>> PackedGaussians::readRawVertex(size_t offset, std::span<const uint8_t> vertexData, const std::map<std::string, std::string>& propertyTypes, std::map<int, std::string> intKeys) {
    std::map<std::string, float> rawVertex;

    for (const auto& [key, property] : intKeys) {
        std::string propertyType = propertyTypes.at(property);
        if (propertyType == "float") {
            float value;
            std::memcpy(&value, vertexData.data() + offset, sizeof(float));
            rawVertex[property] = value;
            offset += sizeof(float);
        }
        else if (propertyType == "uchar") {
            rawVertex[property] = vertexData[offset] / 255.0f;
            offset += sizeof(uint8_t);
        }
    }

    return { offset, rawVertex };
}

int PackedGaussians::nShCoeffs() const {
    switch (sphericalHarmonicsDegree) {
    case 0: return 1;
    case 1: return 4;
    case 2: return 9;
    case 3: return 16;
    default: throw std::invalid_argument("Unsupported SH degree");
    }
}

std::map<std::string, std::vector<float>> PackedGaussians::arrangeVertex(const std::map<std::string, float>& rawVertex, const std::vector<std::string>& shFeatureOrder) {
    std::vector<std::vector<float>> shCoeffs;
    for (int i = 0; i < nShCoeffs(); ++i) {
        std::vector<float> coeff;
        for (int j = 0; j < 3; ++j) {
            coeff.push_back(rawVertex.at(shFeatureOrder[i * 3 + j]));
        }
        shCoeffs.push_back(coeff);
    }

    std::map<std::string, std::vector<float>> arrangedVertex;
    arrangedVertex["position"] = { rawVertex.at("x"), rawVertex.at("y"), rawVertex.at("z") };
    arrangedVertex["logScale"] = { rawVertex.at("scale_0"), rawVertex.at("scale_1"), rawVertex.at("scale_2") };
    arrangedVertex["rotQuat"] = { rawVertex.at("rot_0"), rawVertex.at("rot_1"), rawVertex.at("rot_2"), rawVertex.at("rot_3") };
    arrangedVertex["opacityLogit"] = { rawVertex.at("opacity") };
    arrangedVertex["shCoeffs"] = std::vector<float>();
    for (const auto& coeff : shCoeffs) {
        arrangedVertex["shCoeffs"].insert(arrangedVertex["shCoeffs"].end(), coeff.begin(), coeff.end());
    }

    return arrangedVertex;
}

PackedGaussians::PackedGaussians(std::span<const uint8_t> arrayBuffer) {
    auto [vertexCount, propertyTypes, vertexData, intKeys] = decodeHeader(arrayBuffer);
    numGaussians = vertexCount;

    int nRestCoeffs = 0;
    for (const auto& [propertyName, propertyType] : propertyTypes) {
        if (propertyName.find("f_rest_") == 0) {
            nRestCoeffs++;
        }
    }
    int nCoeffsPerColor = nRestCoeffs / 3;
    sphericalHarmonicsDegree = static_cast<int>(std::sqrt(nCoeffsPerColor + 1) - 1);
    std::cout << "Detected degree " << sphericalHarmonicsDegree << " with " << nCoeffsPerColor << " coefficients per color\n";

    std::vector<std::string> shFeatureOrder;
    for (int rgb = 0; rgb < 3; ++rgb) {
        shFeatureOrder.push_back("f_dc_" + std::to_string(rgb));
    }
    for (int i = 0; i < nCoeffsPerColor; ++i) {
        for (int rgb = 0; rgb < 3; ++rgb) {
            shFeatureOrder.push_back("f_rest_" + std::to_string(rgb * nCoeffsPerColor + i));
        }
    }

    size_t readOffset = 0;
    for (int i = 0; i < vertexCount; ++i) {
        auto [newReadOffset, rawVertex] = readRawVertex(readOffset, vertexData, propertyTypes, intKeys);
        readOffset = newReadOffset;

        auto arrangedVertex = arrangeVertex(rawVertex, shFeatureOrder);
        positions.push_back({ arrangedVertex["position"][0], arrangedVertex["position"][1], arrangedVertex["position"][2] });
        logScales.push_back({ arrangedVertex["logScale"][0], arrangedVertex["logScale"][1], arrangedVertex["logScale"][2] });
        rotQuats.push_back({ arrangedVertex["rotQuat"][0], arrangedVertex["rotQuat"][1], arrangedVertex["rotQuat"][2], arrangedVertex["rotQuat"][3] });
        opacityLogits.push_back(arrangedVertex["opacityLogit"][0]);

        std::vector<std::array<float, 3>> coeffs;
        for (int j = 0; j < nShCoeffs(); ++j) {
            coeffs.push_back({ arrangedVertex["shCoeffs"][j * 3], arrangedVertex["shCoeffs"][j * 3 + 1], arrangedVertex["shCoeffs"][j * 3 + 2] });
        }
        shCoeffs.push_back(coeffs);
    }
}

std::vector<uint8_t> loadFileAsArrayBuffer(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to load file");
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        throw std::runtime_error("Failed to read file");
    }

    return buffer;
}
