#!/usr/bin/env python3
"""
Demo-video auto-edit helper.

Reads one or more events_<pid>.jsonl files produced by the client
EventLogger, merges them onto a single timeline, applies per-tag cut
rules, and drives ffmpeg to produce a concatenated demo video.

Two input modes:

1) Simple: one global offset applied to every events file.

       python tools/auto_edit.py                              \
           --video    raw.mp4                                 \
           --events   run/P1/events_*.jsonl run/P2/events_*.jsonl \
           --out      demo.mp4                                \
           --offset   0.0

2) Per-source offsets via a manifest JSON (used by record_demo.py):

       python tools/auto_edit.py                              \
           --manifest recording_info.json                     \
           --out      demo.mp4

   manifest shape:
       {
           "video": "raw.mp4",
           "sources": [
               {"glob": "run/P1/events_*.jsonl", "offset": 2.15},
               {"glob": "run/P2/events_*.jsonl", "offset": 2.47}
           ]
       }

   Each source's offset is the number of seconds between the start of
   the OBS recording and the launch of that particular client
   instance. record_demo.py computes this automatically.
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

# (pre_seconds, post_seconds) around each tag. Negative pre = clip starts
# before the event; post = clip length after the event.
DEFAULT_RULES = {
    "recording_start": None,         # skip (marker only)
    "recording_end":   None,
    "scene_enter":     (-2.0, 6.0),  # entering game scene
    "dungeon_built":   (-1.0, 8.0),  # instanced rendering shot
    "remote_spawn":    (-1.0, 10.0), # multiplayer sync showcase
    "first_fire":      (-1.0, 4.0),  # combat start
    "first_hit":       None,         # redundant with first_kill
    "first_kill":      (-2.0, 5.0),  # kill highlight
    "portal":          (-3.0, 8.0),  # zone transition
}


def load_events(sources):
    """sources: list of (glob_pattern, offset_seconds) tuples."""
    events = []
    for pattern, offset in sources:
        matched = sorted(glob.glob(pattern))
        if not matched:
            print(f"[warn] no files matched: {pattern}", file=sys.stderr)
        for path in matched:
            src = Path(path).stem  # e.g. "events_1234"
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        ev = json.loads(line)
                    except json.JSONDecodeError:
                        print(f"[warn] bad json in {path}: {line}", file=sys.stderr)
                        continue
                    ev["source"] = src
                    ev["t"] = ev["ms"] / 1000.0 + offset
                    events.append(ev)
    events.sort(key=lambda e: e["t"])
    return events


def dedupe(events):
    """For duplicate tags across sources, keep the earliest occurrence.
    This handles the case where both clients log `dungeon_built` simultaneously:
    we only want one clip for it."""
    seen = set()
    result = []
    for ev in events:
        key = (ev["tag"], ev.get("note", ""))
        # Only dedupe "first_*" and once-per-scene tags. Allow repeats of portal etc.
        if ev["tag"] in ("first_fire", "first_hit", "first_kill",
                         "remote_spawn", "scene_enter", "dungeon_built"):
            if key in seen:
                continue
            seen.add(key)
        result.append(ev)
    return result


def build_clips(events, video_path, rules, out_dir):
    clips = []
    for ev in events:
        rule = rules.get(ev["tag"])
        if rule is None:
            continue
        pre, post = rule
        start = max(0.0, ev["t"] + pre)
        dur = post - pre
        clip = out_dir / f"clip_{len(clips):03d}_{ev['tag']}.mp4"
        print(f"[cut] {ev['tag']:15s} @ {ev['t']:7.2f}s  →  "
              f"{start:7.2f}+{dur:.2f}s  ({clip.name})")
        cmd = [
            "ffmpeg", "-y", "-loglevel", "error",
            "-ss", f"{start:.3f}",
            "-i", str(video_path),
            "-t", f"{dur:.3f}",
            "-c:v", "libx264", "-preset", "fast", "-crf", "18",
            "-c:a", "aac", "-b:a", "160k",
            str(clip),
        ]
        subprocess.run(cmd, check=True)
        clips.append(clip)
    return clips


def concat(clips, out_path):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        for c in clips:
            f.write(f"file '{c.resolve().as_posix()}'\n")
        listfile = f.name
    try:
        subprocess.run([
            "ffmpeg", "-y", "-loglevel", "error",
            "-f", "concat", "-safe", "0",
            "-i", listfile,
            "-c", "copy",
            str(out_path),
        ], check=True)
    finally:
        os.unlink(listfile)


def run_edit(video, sources, out, work_dir):
    """Library entry point. sources = list of (glob, offset) tuples."""
    video = Path(video)
    if not video.exists():
        raise FileNotFoundError(f"video not found: {video}")

    events = load_events(sources)
    if not events:
        raise RuntimeError("no events loaded")

    events = dedupe(events)
    print(f"[info] {len(events)} events on merged timeline")

    work = Path(work_dir)
    work.mkdir(parents=True, exist_ok=True)

    clips = build_clips(events, video, DEFAULT_RULES, work)
    if not clips:
        raise RuntimeError("no clips produced (check your rules / events)")

    concat(clips, Path(out))
    print(f"[done] wrote {out} ({len(clips)} clips)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--video", help="OBS recording (mp4). Omit when using --manifest.")
    ap.add_argument("--events", nargs="+",
                    help="Path(s) or glob(s) to events_*.jsonl files. "
                         "Uses --offset for all of them.")
    ap.add_argument("--manifest",
                    help="JSON file with per-source offsets "
                         "(produced by record_demo.py).")
    ap.add_argument("--out", default="demo.mp4", help="Output video path.")
    ap.add_argument("--offset", type=float, default=0.0,
                    help="Seconds to add to every event timestamp before cutting. "
                         "Only used with --events.")
    ap.add_argument("--work", default="_autoedit",
                    help="Directory for intermediate clip files.")
    args = ap.parse_args()

    if args.manifest:
        with open(args.manifest, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        video = manifest["video"]
        sources = [(s["glob"], float(s.get("offset", 0.0)))
                   for s in manifest["sources"]]
    else:
        if not args.video or not args.events:
            ap.error("either --manifest, or both --video and --events are required")
        video = args.video
        sources = [(p, args.offset) for p in args.events]

    try:
        run_edit(video, sources, args.out, args.work)
    except (FileNotFoundError, RuntimeError) as e:
        sys.exit(f"[error] {e}")


if __name__ == "__main__":
    main()
