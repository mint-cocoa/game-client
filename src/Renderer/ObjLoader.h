#pragma once
#include "Renderer/Mesh.h"
#include <string>
#include <vector>
#include <cstdint>

class ObjLoader {
public:
    static bool Load(const std::string& path,
                     std::vector<Vertex>& outVertices,
                     std::vector<uint32_t>& outIndices);

    static Mesh CreateMesh(ID3D11Device* device,
                           const std::vector<Vertex>& vertices,
                           const std::vector<uint32_t>& indices);
};
