#pragma once

class DX11Device;
class Pipeline;
class Camera;
class MeshCache;
class MaterialManager;
class Input;
class UIManager;
#include <string>

struct EngineContext {
    DX11Device*      device      = nullptr;
    Pipeline*        pipeline    = nullptr;
    Camera*          camera      = nullptr;
    MeshCache*       meshCache   = nullptr;
    MaterialManager* materialMgr = nullptr;
    Input*           input       = nullptr;
    UIManager*       uiManager   = nullptr;
    const std::string* assetsDir = nullptr;
    bool pipelineOk              = false;
};
