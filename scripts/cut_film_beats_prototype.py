#!/usr/bin/env python3
"""PROTOTYPE, throwaway (branch feat/ignite-tour-prototype): the film cut for the drive.

Cuts each beat of a recorded film into a clip of its own, with no captions, for the card that
shows it: the same trim, pointer and camera as the film's edit in record_film.py, without the
fades between beats or the burned-in lines, since the card's title says what the caption did.
Each clip gets a poster of its first frame, so a card approaching on the road already shows its
footage.

    scripts/cut_film_beats_prototype.py [build/film]

Writes website/assets/film/beats/<beat>.webm, .mp4 and .webp.
"""

import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import record_film as film  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "website" / "assets" / "film" / "beats"

# The card each beat plays on, by the beat's name in marks.json.
CARDS = {
    "Omnibar": "omnibar",
    "Hints": "keyboard",
    "Space switch": "spaces",
    "Sidebar": "pros",
    "Blocking": "blocking",
    "Theme": "theme",
    "Agent": "agents",
}


def main() -> int:
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "film"
    marks = json.loads((source / "marks.json").read_text(encoding="utf-8"))
    placed = source / film.STAGE
    window = json.loads(placed.read_text(encoding="utf-8"))["window"] if placed.exists() else None
    stage = film.staging(film.recording_size(source / "raw.mkv"), window)
    drawn = film.DRAWN_POINTER
    OUT.mkdir(parents=True, exist_ok=True)
    for entry in marks:
        card = CARDS.get(entry["beat"])
        if not card:
            continue
        beat = film.BEAT_NAMED[entry["beat"]]
        chain = (f"[0:v]trim=start={entry['start']:.3f}:end={entry['end']:.3f},"
                 f"setpts=PTS-STARTPTS{stage.scaling}")
        overlay = film.pointer(entry, drawn.hot, stage.origin)
        inputs = ["-i", str(source / "raw.mkv")]
        if overlay:
            inputs += ["-i", str(drawn.path)]
            chain = f"{chain}[r];[r][1:v]{overlay}"
        focus = [(at, tuple(place) if isinstance(place, list) else place)
                 for at, place in entry.get("focus", beat.focus)]
        chain += (f",{film.camera(focus, stage.window)},setsar=1,format=yuv420p,"
                  f"scale=1280:-2,{film.COLOURS}[clip]")
        master = OUT / f"{card}.mkv"
        subprocess.run(["ffmpeg", "-v", "error", "-y", *inputs, "-filter_complex", chain,
                        "-map", "[clip]", "-an", "-c:v", "libx264", "-preset", "veryfast",
                        "-crf", "10", str(master)], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-an",
                        "-c:v", "libvpx-vp9", "-b:v", "0", "-crf", "38", "-row-mt", "1",
                        "-deadline", "good", "-cpu-used", "3", str(OUT / f"{card}.webm")],
                       check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-an",
                        "-c:v", "libx264", "-preset", "slow", "-crf", "28",
                        "-movflags", "+faststart", str(OUT / f"{card}.mp4")], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-frames:v", "1",
                        "-c:v", "libwebp", "-quality", "80", str(OUT / f"{card}.webp")],
                       check=True)
        master.unlink()
        print(f"{card}: {entry['end'] - entry['start']:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
