#include "Renderer/ObjLoader.h"
#include <fstream>
#include <sstream>
#include <map>
#include <tuple>
#include <algorithm>

bool ObjLoader::Load(const std::string& path,
                     std::vector<Vertex>& outVertices,
                     std::vector<uint32_t>& outIndices)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT2> uvs;

    // Dedup map: (posIdx, uvIdx, normIdx) -> vertex index
    std::map<std::tuple<int,int,int>, uint32_t> vertexMap;

    outVertices.clear();
    outIndices.clear();

    std::string line;
    while (std::getline(file, line)) {
        // Strip carriage return
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            // Kenney OBJ may have extra vertex color values — just ignore them
            positions.push_back({ x, y, z });
        }
        else if (prefix == "vn") {
            float x, y, z;
            iss >> x >> y >> z;
            normals.push_back({ x, y, z });
        }
        else if (prefix == "vt") {
            float u, v;
            iss >> u >> v;
            uvs.push_back({ u, 1.0f - v });  // OBJ V is bottom-up, DX11 is top-down
        }
        else if (prefix == "f") {
            // Parse face vertices
            std::vector<uint32_t> faceIndices;
            std::string token;
            while (iss >> token) {
                int vi = 0, ti = 0, ni = 0;

                // Parse v/vt/vn, v//vn, v/vt, v
                size_t s1 = token.find('/');
                if (s1 == std::string::npos) {
                    vi = std::stoi(token);
                } else {
                    vi = std::stoi(token.substr(0, s1));
                    size_t s2 = token.find('/', s1 + 1);
                    if (s2 == std::string::npos) {
                        // v/vt
                        ti = std::stoi(token.substr(s1 + 1));
                    } else {
                        // v/vt/vn or v//vn
                        std::string vtStr = token.substr(s1 + 1, s2 - s1 - 1);
                        if (!vtStr.empty()) ti = std::stoi(vtStr);
                        std::string vnStr = token.substr(s2 + 1);
                        if (!vnStr.empty()) ni = std::stoi(vnStr);
                    }
                }

                // Convert 1-based to 0-based (negative indices not handled)
                int pi = vi - 1;
                int ui = ti > 0 ? ti - 1 : -1;
                int nii = ni > 0 ? ni - 1 : -1;

                auto key = std::make_tuple(pi, ui, nii);
                auto it = vertexMap.find(key);
                if (it != vertexMap.end()) {
                    faceIndices.push_back(it->second);
                } else {
                    Vertex vert = {};
                    if (pi >= 0 && pi < (int)positions.size())
                        vert.Position = positions[pi];
                    if (ui >= 0 && ui < (int)uvs.size())
                        vert.UV = uvs[ui];
                    if (nii >= 0 && nii < (int)normals.size())
                        vert.Normal = normals[nii];

                    uint32_t idx = (uint32_t)outVertices.size();
                    outVertices.push_back(vert);
                    vertexMap[key] = idx;
                    faceIndices.push_back(idx);
                }
            }

            // Fan triangulation for 3+ vertex faces
            for (size_t i = 2; i < faceIndices.size(); i++) {
                outIndices.push_back(faceIndices[0]);
                outIndices.push_back(faceIndices[i - 1]);
                outIndices.push_back(faceIndices[i]);
            }
        }
    }

    // If no normals were in the file, generate flat normals per triangle
    if (normals.empty() && !outIndices.empty()) {
        // Zero all normals first
        for (auto& v : outVertices)
            v.Normal = { 0, 0, 0 };

        for (size_t i = 0; i + 2 < outIndices.size(); i += 3) {
            auto& v0 = outVertices[outIndices[i]];
            auto& v1 = outVertices[outIndices[i + 1]];
            auto& v2 = outVertices[outIndices[i + 2]];

            DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.Position);
            DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.Position);
            DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&v2.Position);

            DirectX::XMVECTOR e1 = DirectX::XMVectorSubtract(p1, p0);
            DirectX::XMVECTOR e2 = DirectX::XMVectorSubtract(p2, p0);
            DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(e1, e2));

            DirectX::XMFLOAT3 nf;
            DirectX::XMStoreFloat3(&nf, n);

            // Accumulate (simple averaging)
            v0.Normal.x += nf.x; v0.Normal.y += nf.y; v0.Normal.z += nf.z;
            v1.Normal.x += nf.x; v1.Normal.y += nf.y; v1.Normal.z += nf.z;
            v2.Normal.x += nf.x; v2.Normal.y += nf.y; v2.Normal.z += nf.z;
        }

        // Normalize accumulated normals
        for (auto& v : outVertices) {
            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&v.Normal);
            n = DirectX::XMVector3Normalize(n);
            DirectX::XMStoreFloat3(&v.Normal, n);
        }
    }

    return !outVertices.empty();
}

Mesh ObjLoader::CreateMesh(ID3D11Device* device,
                           const std::vector<Vertex>& vertices,
                           const std::vector<uint32_t>& indices)
{
    Mesh mesh;
    mesh.indexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = (UINT)(vertices.size() * sizeof(Vertex));
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vsd = {};
    vsd.pSysMem = vertices.data();
    device->CreateBuffer(&vbd, &vsd, &mesh.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA isd = {};
    isd.pSysMem = indices.data();
    device->CreateBuffer(&ibd, &isd, &mesh.indexBuffer);

    return mesh;
}
