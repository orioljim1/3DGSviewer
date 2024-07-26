#ifndef PLY_H
#define PLY_H

#include <vector>
#include <string>
#include <map>
#include <tuple>
#include <span>
#include <array>
#include <stdexcept>

class PackedGaussians {
public:
    int numGaussians;
    int sphericalHarmonicsDegree;

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> logScales;
    std::vector<std::array<float, 4>> rotQuats;
    std::vector<float> opacityLogits;
    std::vector<std::vector<std::array<float, 3>>> shCoeffs;

    static std::tuple<int, std::map<std::string, std::string>, std::span<const uint8_t>, std::map<int, std::string>> decodeHeader(std::span<const uint8_t> plyArrayBuffer);

    std::tuple<size_t, std::map<std::string, float>> readRawVertex(size_t offset, std::span<const uint8_t> vertexData, const std::map<std::string, std::string>& propertyTypes, std::map<int, std::string> intKeys);

    int nShCoeffs() const;

    std::map<std::string, std::vector<float>> arrangeVertex(const std::map<std::string, float>& rawVertex, const std::vector<std::string>& shFeatureOrder);

    PackedGaussians(std::span<const uint8_t> arrayBuffer);
};

std::vector<uint8_t> loadFileAsArrayBuffer(const std::string& filePath);

#endif // PLY_H
