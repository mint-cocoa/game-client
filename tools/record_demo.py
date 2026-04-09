#!/usr/bin/env python3
"""
Orchestrated demo recording + auto-edit driver.

This script:
    1. Connects to OBS via the obs-websocket protocol.
    2. Starts OBS recording and notes the precise start time (t0).
    3. Launches the GameServer (WSL) + two client instances.
    4. Records each client's launch offset relative to t0.
    5. Waits until the user hits Enter (or until both clients exit).
    6. Stops the OBS recording, retrieves the output file path.
    7. Writes recording_info.json with per-source offsets.
    8. Calls auto_edit.run_edit() to produce demo.mp4.

Setup (one-time):

    # Install the Python OBS-WebSocket client
    pip install obsws-python

    # In OBS:
    #   Tools → WebSocket Server Settings
    #   Enable WebSocket server → port 4455
    #   Server password (optional; match --obs-password below)

Run:

    python tools/record_demo.py              \
        --obs-host     localhost             \
        --obs-port     4455                  \
        --obs-password ""                    \
        --server-cmd   "cd ~/servercore_v4/build && ./examples/GameServer/GameServer" \
        --work         run                   \
        --out          demo.mp4

When the game sessions are done, press Enter in this terminal and the
script will handle the rest.
"""

import argparse
import ctypes
import json
import subprocess
import sys
import time
from pathlib import Path

try:
    import obsws_python as obs
except ImportError:
    sys.exit("[error] obsws-python not installed. Run: pip install obsws-python")

# Import auto_edit as a library sibling to this file.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import auto_edit  # noqa: E402


DEMO_SCENE = "DemoRecord"


# ---------------------------------------------------------------------------
# OBS scene auto-setup

def setup_obs_scene(client, scene_name, width, height, capture_server=True):
    """Phase 1: Configure canvas resolution and prepare an empty scene.

    * Sets canvas & output resolution to (width*2) x (height*2 if server, else height).
    * Creates (or clears) a dedicated scene.
    * Sets the scene as the active program scene.

    Call add_window_captures() after the client windows are running.
    """
    canvas_w = width * 2
    canvas_h = height * 2 if capture_server else height

    # ── Video settings ──────────────────────────────────────────────
    print(f"[obs] canvas {canvas_w}×{canvas_h}, 60 fps")
    client.set_video_settings(
        base_width=canvas_w, base_height=canvas_h,
        out_width=canvas_w, out_height=canvas_h,
        numerator=60, denominator=1,
    )

    # ── Scene ────────────────────────────────────────────────────────
    scenes = client.get_scene_list()
    existing = [s["sceneName"] for s in scenes.scenes]

    if scene_name in existing:
        items = client.get_scene_item_list(scene_name)
        for item in items.scene_items:
            client.remove_scene_item(scene_name, item["sceneItemId"])
        print(f"[obs] cleared scene '{scene_name}'")
    else:
        client.create_scene(scene_name)
        print(f"[obs] created scene '{scene_name}'")

    client.set_current_program_scene(scene_name)
    print(f"[obs] scene '{scene_name}' ready (empty, awaiting clients)")


def add_window_captures(client, scene_name, width, height, capture_server=True):
    """Phase 2: Add window_capture sources after the client windows are up.

    Waits briefly for the windows to appear, then creates OBS sources
    matching each client (and optionally server terminal) by title.
    """
    # ── Client windows (top row) ────────────────────────────────────
    clients_cfg = [
        ("Client P1", 0.0),
        ("Client P2", float(width)),
    ]
    for title, pos_x in clients_cfg:
        resp = client.create_input(
            scene_name, title, "window_capture",
            {
                "window": f"{title}:IsometricClient:IsometricClient.exe",
                "capture_cursor": True,
                "method": 2,  # Windows 10 (1903+) capture method
            },
            True,
        )
        client.set_scene_item_transform(
            scene_name, resp.scene_item_id,
            {
                "positionX": pos_x,
                "positionY": 0.0,
                "boundsType": "OBS_BOUNDS_STRETCH",
                "boundsWidth": float(width),
                "boundsHeight": float(height),
            },
        )
        print(f"[obs] window capture '{title}' at x={pos_x}")

    # ── Server terminal (bottom row, full width) ────────────────────
    if capture_server:
        resp = client.create_input(
            scene_name, "GameServer", "window_capture",
            {
                "window": "GameServer:CASCADIA_HOSTING_WINDOW_CLASS:WindowsTerminal.exe",
                "capture_cursor": False,
                "method": 2,
            },
            True,
        )
        client.set_scene_item_transform(
            scene_name, resp.scene_item_id,
            {
                "positionX": 0.0,
                "positionY": float(height),
                "boundsType": "OBS_BOUNDS_STRETCH",
                "boundsWidth": float(width * 2),
                "boundsHeight": float(height),
            },
        )
        print(f"[obs] window capture 'GameServer' at y={height}")

    print(f"[obs] captures bound")


# ---------------------------------------------------------------------------
# Launch helpers

def find_and_resize_window(title, x, y, w, h, retries=10, interval=0.5):
    """Find a window by title and move/resize it to (x, y, w, h) pixels."""
    user32 = ctypes.windll.user32
    for _ in range(retries):
        hwnd = user32.FindWindowW(None, title)
        if hwnd:
            user32.MoveWindow(hwnd, x, y, w, h, True)
            print(f"[win32] resized '{title}' → {w}×{h} at ({x},{y})")
            return True
        time.sleep(interval)
    print(f"[win32] window '{title}' not found after {retries} retries")
    return False


def launch_server(server_cmd):
    """Start the GameServer inside WSL in a separate Windows Terminal window."""
    wt_cmd = [
        "wt", "--window", "new", "--title", "GameServer",
        "wsl", "-d", "Ubuntu-24.04", "--",
        "bash", "-lc", f"{server_cmd} 2>&1 | tee ~/gameserver.log",
    ]
    subprocess.Popen(wt_cmd)


def launch_client(exe, title, work_dir, x, y, w, h):
    """Launch an IsometricClient instance with a given title and work dir."""
    work_dir = Path(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)
    return subprocess.Popen(
        [str(exe), "-t", title,
         "-w", str(w), "-h", str(h),
         "-x", str(x), "-y", str(y)],
        cwd=str(work_dir),
    )


# ---------------------------------------------------------------------------
# Main flow

def main():
    root = Path(__file__).resolve().parents[1]  # isometric_client/

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obs-host", default="localhost")
    ap.add_argument("--obs-port", type=int, default=4455)
    ap.add_argument("--obs-password", default="")
    ap.add_argument("--exe", default=str(root / "x64" / "Debug" / "IsometricClient.exe"),
                    help="Path to IsometricClient.exe")
    ap.add_argument("--server-cmd",
                    default="cd /home/cocoa/servercore_v4/.worktrees/gameserver-observability/build/bin && ./GameServer",
                    help="Shell command (inside WSL) to launch the game server.")
    ap.add_argument("--no-capture-server", action="store_true",
                    help="Don't add server terminal to the OBS capture.")
    ap.add_argument("--skip-server", action="store_true",
                    help="Don't start the server — assume it's already running.")
    ap.add_argument("--work", default=str(root / "run"),
                    help="Directory that will contain run/P1, run/P2 sub-folders.")
    ap.add_argument("--width",  type=int, default=960)
    ap.add_argument("--height", type=int, default=540)
    ap.add_argument("--server-warmup", type=float, default=2.0,
                    help="Seconds to wait after launching the server.")
    ap.add_argument("--out", default="demo.mp4")
    ap.add_argument("--no-edit", action="store_true",
                    help="Stop after recording; skip auto_edit invocation.")
    ap.add_argument("--scene-name", default=DEMO_SCENE,
                    help="OBS scene name to create/use (default: DemoRecord).")
    ap.add_argument("--skip-setup", action="store_true",
                    help="Skip automatic OBS scene/source configuration.")
    ap.add_argument("--setup-only", action="store_true",
                    help="Only configure OBS scene, then exit (no recording).")
    args = ap.parse_args()

    exe = Path(args.exe)
    if not exe.exists():
        sys.exit(f"[error] client exe not found: {exe}\n"
                 f"        build the Debug x64 configuration first.")

    # 1) Connect to OBS
    print(f"[obs] connecting to {args.obs_host}:{args.obs_port}")
    try:
        client = obs.ReqClient(host=args.obs_host, port=args.obs_port,
                               password=args.obs_password, timeout=5)
    except Exception as e:
        sys.exit(f"[error] OBS connect failed: {e}\n"
                 f"        Make sure OBS is running and WebSocket server is enabled.")

    capture_server = not args.no_capture_server

    # 1.5) Auto-configure OBS scene
    if not args.skip_setup:
        setup_obs_scene(client, args.scene_name, args.width, args.height,
                        capture_server=capture_server)

    if args.setup_only:
        print("[info] --setup-only: OBS scene configured. Exiting.")
        return

    status = client.get_record_status()
    if status.output_active:
        sys.exit("[error] OBS is already recording. Stop the current recording first.")

    # 2) Start recording — capture t0 immediately after the call returns
    print("[obs] start_record")
    client.start_record()
    t0 = time.monotonic()

    # 3) Server
    if not args.skip_server:
        print("[server] launching via WSL")
        launch_server(args.server_cmd)
        # Position server terminal to match bottom row of canvas
        canvas_w = args.width * 2
        find_and_resize_window("GameServer", 0, args.height, canvas_w, args.height)
        print(f"[server] waiting {args.server_warmup}s for bind")
        time.sleep(args.server_warmup)

    # 4) Two clients
    work = Path(args.work)
    print("[client] launching Client P1")
    p1 = launch_client(exe, "Client P1", work / "P1",
                       x=0, y=0, w=args.width, h=args.height)
    t_p1 = time.monotonic()

    print("[client] launching Client P2")
    p2 = launch_client(exe, "Client P2", work / "P2",
                       x=args.width, y=0, w=args.width, h=args.height)
    t_p2 = time.monotonic()

    # 4.5) Wait for windows to appear, then bind OBS captures
    if not args.skip_setup:
        print("[obs] waiting 2s for client windows to appear...")
        time.sleep(2)
        add_window_captures(client, args.scene_name, args.width, args.height,
                            capture_server=capture_server)

    offset_p1 = t_p1 - t0
    offset_p2 = t_p2 - t0
    print(f"[info] client offsets: P1={offset_p1:.3f}s  P2={offset_p2:.3f}s")

    # 5) Wait for the user, or until both clients exit
    print()
    print("=" * 60)
    print(" Recording in progress.")
    print(" Play the demo session, then press Enter here to stop.")
    print(" (Or close both client windows and press Enter.)")
    print("=" * 60)
    try:
        input()
    except KeyboardInterrupt:
        print("\n[info] interrupt received, stopping")

    # Make sure clients are gone so events_*.jsonl is flushed
    for proc, name in ((p1, "P1"), (p2, "P2")):
        if proc.poll() is None:
            print(f"[client] terminating {name}")
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()

    # 6) Stop recording and get the video path
    print("[obs] stop_record")
    resp = client.stop_record()
    video_path = getattr(resp, "output_path", None)
    if not video_path:
        # Fallback for older obs-websocket responses
        video_path = client.get_record_status().output_path
    video_path = Path(video_path)
    print(f"[obs] recording saved to {video_path}")

    # 7) Write manifest
    manifest = {
        "video": str(video_path),
        "sources": [
            {"glob": str(work / "P1" / "events_*.jsonl"), "offset": offset_p1},
            {"glob": str(work / "P2" / "events_*.jsonl"), "offset": offset_p2},
        ],
    }
    manifest_path = root / "recording_info.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"[info] manifest: {manifest_path}")

    # 8) Auto-edit
    if args.no_edit:
        print("[info] --no-edit given, skipping auto_edit")
        return

    print("[edit] running auto_edit")
    try:
        auto_edit.run_edit(
            video=manifest["video"],
            sources=[(s["glob"], s["offset"]) for s in manifest["sources"]],
            out=args.out,
            work_dir=str(root / "_autoedit"),
        )
    except Exception as e:
        sys.exit(f"[error] auto_edit failed: {e}")


if __name__ == "__main__":
    main()
