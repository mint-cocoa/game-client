#include "Renderer/MaterialManager.h"

void MaterialManager::Init(ID3D11Device* device, const std::string& texturesDir)
{
    // Create fallback material (1x1 magenta)
    defaultMat_.name = "__fallback__";
    defaultMat_.texture = TextureLoader::CreateFallback(device);

    // Predefined materials
    CreateMaterial(device, "M_Dungeon", texturesDir + "/colormap.png");
    CreateMaterial(device, "M_LocalPlayer", texturesDir + "/texture-a.png");
    CreateMaterial(device, "M_RemotePlayer", texturesDir + "/colormap.png");
    CreateMaterial(device, "M_Bullet", texturesDir + "/particle.png");
}

void MaterialManager::CreateMaterial(ID3D11Device* device, const std::string& name, const std::string& texturePath)
{
    Material mat;
    mat.name = name;
    mat.texture = TextureLoader::LoadFromFile(device, texturePath);
    if (!mat.texture) {
        mat.texture = defaultMat_.texture;  // fallback to magenta
    }
    materials_[name] = std::move(mat);
}

const Material* MaterialManager::Get(const std::string& name) const
{
    auto it = materials_.find(name);
    if (it != materials_.end()) return &it->second;
    return nullptr;
}

void MaterialManager::Bind(ID3D11DeviceContext* ctx, const Material* mat) const
{
    if (!mat || !mat->texture) return;
    ID3D11ShaderResourceView* srv = mat->texture.Get();
    ctx->PSSetShaderResources(0, 1, &srv);
}
