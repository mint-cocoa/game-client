#pragma once
#include "Renderer/Mesh.h"
#include "Renderer/ObjLoader.h"
#include <unordered_map>
#include <string>

class MeshCache {
public:
    void Init(ID3D11Device* device, const std::string& modelsDir, const std::string& fbxDir = "");
    bool LoadFbx(ID3D11Device* device, const std::string& path, const std::string& name);
    const Mesh* Get(const std::string& name) const;

    void GenerateBoxMesh(ID3D11Device* device, const std::string& name,
                         float halfW, float height, float halfD);
    void GenerateSphereMesh(ID3D11Device* device, const std::string& name,
                            float radius, int slices = 12, int stacks = 8);

    // Material assignment per mesh
    void SetMeshMaterial(const std::string& meshName, const std::string& materialName);
    const std::string& GetMeshMaterial(const std::string& meshName) const;

private:
    std::unordered_map<std::string, Mesh> meshes_;
    std::unordered_map<std::string, std::string> meshMaterials_;
    static const std::string emptyString_;
};
