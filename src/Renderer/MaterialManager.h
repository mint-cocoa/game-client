#pragma once
#include "Renderer/Material.h"
#include "Renderer/TextureLoader.h"
#include <unordered_map>
#include <string>

class MaterialManager {
public:
    void Init(ID3D11Device* device, const std::string& texturesDir);

    // Create a named material with a texture file
    void CreateMaterial(ID3D11Device* device, const std::string& name, const std::string& texturePath);

    // Get material by name (returns nullptr if not found)
    const Material* Get(const std::string& name) const;

    // Bind material's texture to pixel shader slot 0
    void Bind(ID3D11DeviceContext* ctx, const Material* mat) const;

    // Get default fallback material
    const Material* GetDefault() const { return &defaultMat_; }

private:
    std::unordered_map<std::string, Material> materials_;
    Material defaultMat_;  // magenta fallback
};
