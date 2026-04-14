# Isometric Game Client

DirectX 11 기반 아이소메트릭 멀티플레이어 던전 크롤러 게임 클라이언트입니다.  
서버(ServerCore v4)와 TCP/Protobuf 프로토콜로 통신하며, 실시간 전투, 인벤토리, 파티, 채팅 등의 기능을 제공합니다.

## Tech Stack

| 분류 | 기술 |
|------|------|
| Language | C++20 (MSVC v143) |
| Graphics | Direct3D 11 |
| UI | ImGui |
| Networking | Winsock2 + IOCP (비동기, 단일 스레드) |
| Serialization | Protocol Buffers 3 |
| Asset Loading | Assimp, stb_image |

## Build

### Prerequisites

- **Visual Studio 2022+** (C++ 워크로드, MSVC v143)
- **vcpkg** (`C:\vcpkg\`)에 다음 패키지 설치:
  - `protobuf`
  - `assimp`
- **Windows SDK** (DirectX 11 포함)

### Build

```bash
# Visual Studio
IsometricClient.sln 열기 -> x64 Debug/Release -> 빌드

# Command Line
msbuild IsometricClient.sln /p:Configuration=Debug /p:Platform=x64
```

출력: `x64\Debug\IsometricClient.exe`

## Run

```bash
# 기본 실행
./x64/Debug/IsometricClient.exe

# 커스텀 윈도우
./x64/Debug/IsometricClient.exe -t "My Client" -w 1920 -h 1080 -x 100 -y 50
```

| 옵션 | 설명 | 기본값 |
|------|------|--------|
| `-t` | 윈도우 타이틀 | "Isometric Client" |
| `-w` | 너비 | 1280 |
| `-h` | 높이 | 720 |
| `-x` | X 위치 | auto |
| `-y` | Y 위치 | auto |

서버 기본 주소: `127.0.0.1:7777` (로그인 화면에서 변경 가능)

### Demo Mode

```bash
# 2개 클라이언트 + WSL 서버 동시 실행
.\run_demo.bat
```

## Controls

| 키 | 동작 |
|----|------|
| WASD | 이동 |
| 좌클릭 | 공격 |
| E | 인벤토리 |
| Tab | 리더보드 |
| Enter | 채팅 |

## Project Structure

```
src/
├── Core/           # 엔진 기반 (App, WinMain, DX11Device, Input, Timer)
├── Renderer/       # 렌더링 (Pipeline, Camera, InstanceRenderer, EffectRenderer, Minimap)
├── Scene/          # 씬 관리 (Login, CharSelect, Lobby, Game)
├── Network/        # TCP/IOCP 네트워킹 (TcpClient, PacketFramer, PacketBuilder)
├── Game/           # 게임 로직 (EntityManager, PlayerController, CombatManager, DungeonGenerator)
├── Data/           # 데이터 구조 (Player, Inventory, Skill, Currency, Chat)
└── UI/             # ImGui UI

proto/              # Protobuf 정의 (Auth, Game, Social, Inventory, Currency)
assets/
├── fbx/            # FBX 소스 모델
├── models/         # OBJ/MTL 런타임 모델
├── shaders/        # HLSL 셰이더 (default, effect, minimap)
└── textures/       # 런타임 텍스처
tools/              # 데모 녹화/편집 스크립트
```

## Architecture

### Rendering

- **아이소메트릭 카메라**: 고정 30 pitch / 45 yaw, 플레이어 추적 + 줌
- **인스턴스 렌더링**: 동일 메시(벽, 바닥, 배럴 등) 배치 렌더링으로 드로우콜 최소화
- **이펙트 렌더러**: 투사체, 피격 파티클
- **미니맵**: 실시간 그리드 시각화

### Networking

- **IOCP 비동기 I/O**: 단일 스레드, ConnectEx/WSARecv/WSASend, 프레임당 1회 완료 폴링
- **패킷 구조**: `[2B size][2B msgId][protobuf payload]`
- **이동 전송**: 20Hz (50ms 쓰로틀)
- **메시지 핸들러**: 씬 진입 시 등록, 퇴장 시 해제

### Message ID Ranges

| 범위 | 용도 |
|------|------|
| 101-118 | 인증, 룸 관리 |
| 201-219 | 이동, 전투, 스폰, 아이템, 포탈 |
| 301-309 | 채팅, 파티 |
| 501-509 | 인벤토리 |
| 601-604 | 재화 |

### Game Flow

```
Login -> CharSelect -> Lobby -> Game
```

각 씬은 진입 시 패킷 핸들러를 등록하고, GameScene은 SceneReady 메시지로 서버에 준비 완료를 알립니다.

### Combat

1. `C_Attack` -> 서버 검증 -> `S_Attack` (애니메이션 브로드캐스트)
2. `S_Damage` (결과: 데미지, 잔여HP, 사망 여부)
3. 클라이언트: 데미지 팝업, 피격 플래시, HP 바 표시
