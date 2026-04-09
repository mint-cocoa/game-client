#include "Renderer/MeshCache.h"
#include <filesystem>
#include <vector>
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace fs = std::filesystem;

const std::string MeshCache::emptyString_;

void MeshCache::Init(ID3D11Device* device, const std::string& modelsDir, const std::string& fbxDir)
{
    // Load all .obj files from modelsDir
    for (auto& entry : fs::directory_iterator(modelsDir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".obj" && ext != ".OBJ") continue;

        std::string name = entry.path().stem().string();
        std::vector<Vertex> verts;
        std::vector<uint32_t> indices;

        if (ObjLoader::Load(entry.path().string(), verts, indices)) {
            meshes_[name] = ObjLoader::CreateMesh(device, verts, indices);
        }
    }

    // Load all .fbx files — overwrite OBJ if same name exists with 0 indices
    if (!fbxDir.empty() && fs::exists(fbxDir)) {
        for (auto& entry : fs::directory_iterator(fbxDir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".fbx" && ext != ".FBX") continue;

            std::string name = entry.path().stem().string();
            // Skip if OBJ already loaded with valid data
            auto it = meshes_.find(name);
            if (it != meshes_.end() && it->second.indexCount > 0) continue;

            LoadFbx(device, entry.path().string(), name);
        }
    }

    // Generate box mesh for characters if still empty or missing
    if (meshes_.find("character-human") == meshes_.end() || meshes_["character-human"].indexCount == 0) {
        GenerateBoxMesh(device, "character-human", 0.4f, 0.9f, 0.4f);
    }
    if (meshes_.find("character-a") == meshes_.end() || meshes_["character-a"].indexCount == 0) {
        GenerateBoxMesh(device, "character-a", 0.4f, 0.9f, 0.4f);
    }

    // Bullet sphere mesh (matches Unity IsoBullet)
    GenerateSphereMesh(device, "bullet", 0.5f);
}

bool MeshCache::LoadFbx(ID3D11Device* device, const std::string& path, const std::string& name)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

    if (!scene || !scene->HasMeshes()) return false;

    // Compute bounding box height for normalization
    float minY = FLT_MAX, maxY = -FLT_MAX;
    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* m = scene->mMeshes[mi];
        for (unsigned vi = 0; vi < m->mNumVertices; ++vi) {
            float y = m->mVertices[vi].y;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    // Scale so height matches character-human OBJ (~2.7 units)
    float fbxHeight = maxY - minY;
    float scale = (fbxHeight > 0.001f) ? 2.7f / fbxHeight : 1.0f;

    std::vector<Vertex> allVerts;
    std::vector<uint32_t> allIndices;

    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* m = scene->mMeshes[mi];
        uint32_t baseVertex = (uint32_t)allVerts.size();

        for (unsigned vi = 0; vi < m->mNumVertices; ++vi) {
            Vertex v = {};
            v.Position = {
                m->mVertices[vi].x * scale,
                m->mVertices[vi].y * scale,
                m->mVertices[vi].z * scale
            };
            if (m->HasNormals()) {
                v.Normal = { m->mNormals[vi].x, m->mNormals[vi].y, m->mNormals[vi].z };
            }
            if (m->HasTextureCoords(0)) {
                v.UV = { m->mTextureCoords[0][vi].x, 1.0f - m->mTextureCoords[0][vi].y };
            }
            allVerts.push_back(v);
        }

        for (unsigned fi = 0; fi < m->mNumFaces; ++fi) {
            const aiFace& face = m->mFaces[fi];
            if (face.mNumIndices != 3) continue;
            allIndices.push_back(baseVertex + face.mIndices[0]);
            allIndices.push_back(baseVertex + face.mIndices[1]);
            allIndices.push_back(baseVertex + face.mIndices[2]);
        }
    }

    if (allVerts.empty()) return false;

    meshes_[name] = ObjLoader::CreateMesh(device, allVerts, allIndices);
    return true;
}

void MeshCache::GenerateBoxMesh(ID3D11Device* device, const std::string& name,
                                float hw, float h, float hd)
{
    // Box from (-hw, 0, -hd) to (hw, h, hd) — 6 faces, 24 verts, 36 indices
    // UV set to colormap skin color area (~0.65, 0.55)
    float u = 0.65f, v = 0.55f;
    std::vector<Vertex> verts = {
        // Top (+Y)
        {{ -hw, h, -hd }, { 0,1,0 }, { u,v }}, {{ -hw, h, hd }, { 0,1,0 }, { u,v }},
        {{ hw, h, hd }, { 0,1,0 }, { u,v }},    {{ hw, h, -hd }, { 0,1,0 }, { u,v }},
        // Bottom (-Y)
        {{ -hw, 0, hd }, { 0,-1,0 }, { u,v }}, {{ -hw, 0, -hd }, { 0,-1,0 }, { u,v }},
        {{ hw, 0, -hd }, { 0,-1,0 }, { u,v }},  {{ hw, 0, hd }, { 0,-1,0 }, { u,v }},
        // Front (+Z)
        {{ -hw, 0, hd }, { 0,0,1 }, { u,v }},  {{ hw, 0, hd }, { 0,0,1 }, { u,v }},
        {{ hw, h, hd }, { 0,0,1 }, { u,v }},    {{ -hw, h, hd }, { 0,0,1 }, { u,v }},
        // Back (-Z)
        {{ hw, 0, -hd }, { 0,0,-1 }, { u,v }}, {{ -hw, 0, -hd }, { 0,0,-1 }, { u,v }},
        {{ -hw, h, -hd }, { 0,0,-1 }, { u,v }}, {{ hw, h, -hd }, { 0,0,-1 }, { u,v }},
        // Right (+X)
        {{ hw, 0, hd }, { 1,0,0 }, { u,v }},   {{ hw, 0, -hd }, { 1,0,0 }, { u,v }},
        {{ hw, h, -hd }, { 1,0,0 }, { u,v }},   {{ hw, h, hd }, { 1,0,0 }, { u,v }},
        // Left (-X)
        {{ -hw, 0, -hd }, { -1,0,0 }, { u,v }}, {{ -hw, 0, hd }, { -1,0,0 }, { u,v }},
        {{ -hw, h, hd }, { -1,0,0 }, { u,v }},   {{ -hw, h, -hd }, { -1,0,0 }, { u,v }},
    };
    std::vector<uint32_t> indices = {
        0,1,2, 0,2,3,     4,5,6, 4,6,7,
        8,9,10, 8,10,11,  12,13,14, 12,14,15,
        16,17,18, 16,18,19, 20,21,22, 20,22,23
    };
    meshes_[name] = ObjLoader::CreateMesh(device, verts, indices);
}

void MeshCache::GenerateSphereMesh(ID3D11Device* device, const std::string& name,
                                    float radius, int slices, int stacks)
{
    const float PI = 3.14159265f;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    // Generate vertices
    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * (float)i / (float)stacks;
        float sinP = sinf(phi), cosP = cosf(phi);
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * PI * (float)j / (float)slices;
            float sinT = sinf(theta), cosT = cosf(theta);

            float nx = sinP * cosT;
            float ny = cosP;
            float nz = sinP * sinT;

            Vertex v;
            v.Position = { nx * radius, ny * radius, nz * radius };
            v.Normal = { nx, ny, nz };
            v.UV = { (float)j / (float)slices, (float)i / (float)stacks };
            verts.push_back(v);
        }
    }

    // Generate indices
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int a = i * (slices + 1) + j;
            int b = a + slices + 1;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(a + 1);
            indices.push_back(a + 1);
            indices.push_back(b);
            indices.push_back(b + 1);
        }
    }

    meshes_[name] = ObjLoader::CreateMesh(device, verts, indices);
}

const Mesh* MeshCache::Get(const std::string& name) const
{
    auto it = meshes_.find(name);
    if (it != meshes_.end()) return &it->second;
    return nullptr;
}

void MeshCache::SetMeshMaterial(const std::string& meshName, const std::string& materialName)
{
    meshMaterials_[meshName] = materialName;
}

const std::string& MeshCache::GetMeshMaterial(const std::string& meshName) const
{
    auto it = meshMaterials_.find(meshName);
    if (it != meshMaterials_.end()) return it->second;
    return emptyString_;
}
