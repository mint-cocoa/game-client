# Isometric Game Client

A multiplayer dungeon crawler game client built with DirectX 11 and C++.  
Communicates with the backend server (ServerCore v4) via TCP/Protobuf, featuring real-time combat, inventory, party, and chat systems.

## Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 (MSVC v143) |
| Graphics | Direct3D 11 |
| UI | ImGui |
| Networking | Winsock2 + IOCP (async, single-threaded) |
| Serialization | Protocol Buffers 3 |
| Asset Loading | Assimp, stb_image |

## Build

### Prerequisites

- **Visual Studio 2022+** with C++ workload (MSVC v143)
- **vcpkg** at `C:\vcpkg\` with the following packages:
  - `protobuf`
  - `assimp`
- **Windows SDK** (includes DirectX 11)

### Build Instructions

```bash
# Visual Studio
Open IsometricClient.sln -> Select x64 Debug/Release -> Build

# Command Line
msbuild IsometricClient.sln /p:Configuration=Debug /p:Platform=x64
```

Output: `x64\Debug\IsometricClient.exe`

## Run

```bash
# Default
./x64/Debug/IsometricClient.exe

# Custom window
./x64/Debug/IsometricClient.exe -t "My Client" -w 1920 -h 1080 -x 100 -y 50
```

| Flag | Description | Default |
|------|-------------|---------|
| `-t` | Window title | "Isometric Client" |
| `-w` | Width | 1280 |
| `-h` | Height | 720 |
| `-x` | X position | auto |
| `-y` | Y position | auto |

Default server address: `127.0.0.1:7777` (configurable on the login screen)

### Demo Mode

```bash
# Launches 2 clients + WSL server simultaneously
.\run_demo.bat
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Movement |
| Left Click | Attack |
| E | Inventory |
| Tab | Leaderboard |
| Enter | Chat |

## Project Structure

```
src/
├── Core/           # Engine foundation (App, WinMain, DX11Device, Input, Timer)
├── Renderer/       # Rendering (Pipeline, Camera, InstanceRenderer, EffectRenderer, Minimap)
├── Scene/          # Scene management (Login, CharSelect, Lobby, Game)
├── Network/        # TCP/IOCP networking (TcpClient, PacketFramer, PacketBuilder)
├── Game/           # Game logic (EntityManager, PlayerController, CombatManager, DungeonGenerator)
├── Data/           # Data structures (Player, Inventory, Skill, Currency, Chat)
└── UI/             # ImGui UI

proto/              # Protobuf definitions (Auth, Game, Social, Inventory, Currency)
assets/
├── fbx/            # FBX source models
├── models/         # OBJ/MTL runtime models
├── shaders/        # HLSL shaders (default, effect, minimap)
└── textures/       # Runtime textures
tools/              # Demo recording/editing scripts
```

## Architecture

### Rendering

- **Isometric Camera**: Fixed 30° pitch / 45° yaw with player tracking and zoom
- **Instanced Rendering**: Batches identical meshes (walls, floors, barrels, etc.) to minimize draw calls
- **Effect Renderer**: Projectiles and hit particle effects
- **Minimap**: Real-time grid visualization with entity markers

### Networking

- **IOCP Async I/O**: Single-threaded, uses ConnectEx/WSARecv/WSASend with per-frame completion polling
- **Packet Format**: `[2B size][2B msgId][protobuf payload]`
- **Movement Send Rate**: 20 Hz (50ms throttle)
- **Message Handlers**: Registered on scene enter, unregistered on scene exit

### Message ID Ranges

| Range | Purpose |
|-------|---------|
| 101-118 | Authentication, room management |
| 201-219 | Movement, combat, spawning, items, portals |
| 301-309 | Chat, party |
| 501-509 | Inventory |
| 601-604 | Currency |

### Game Flow

```
Login -> CharSelect -> Lobby -> Game
```

Each scene registers its own packet handlers on entry. GameScene sends a SceneReady message to notify the server before receiving broadcasts.

### Combat

1. `C_Attack` -> server validation -> `S_Attack` (animation broadcast)
2. `S_Damage` (result: damage dealt, remaining HP, death flag)
3. Client-side feedback: damage popups, hit flash, HP bar display
