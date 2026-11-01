#!/usr/bin/env python3
"""Compare local GBA ROM screenshots against pinned mGBA captures.

ROMs are supplied by the user and captures are written only to the build tree.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess


WIDTH, HEIGHT = 240, 160


def pixels(path):
    data = path.read_bytes()
    header = f"P6\n{WIDTH} {HEIGHT}\n255\n".encode()
    if not data.startswith(header) or len(data) != len(header) + WIDTH * HEIGHT * 3:
        raise RuntimeError(f"Invalid {WIDTH}x{HEIGHT} PPM capture: {path}")
    return bytes(value >> 3 for value in data[len(header):])


def compare(actual, expected, minimum_colors):
    colors = {tuple(actual[i:i + 3]) for i in range(0, len(actual), 3)}
    if len(colors) < minimum_colors:
        raise RuntimeError(
            f"Capture has {len(colors)} unique colors; expected at least {minimum_colors}")
    mean_error = sum(abs(a - b) for a, b in zip(actual, expected)) / len(actual)
    close = sum(max(abs(actual[i + c] - expected[i + c]) for c in range(3)) <= 2
                for i in range(0, len(actual), 3)) / (WIDTH * HEIGHT)
    return mean_error, close


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", required=True, type=Path)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--frames", default="600",
                        help="comma-separated frame checkpoints (default: 600)")
    parser.add_argument("--alignment-window", type=int, default=20,
                        help="reference frames on either side used for animation alignment")
    parser.add_argument("--max-mean-error", type=float, default=0.5,
                        help="maximum average per-channel error in GBA 5-bit color space")
    parser.add_argument("--min-close-pixels", type=float, default=0.95,
                        help="minimum fraction of pixels within two channel levels")
    parser.add_argument("--min-colors", type=int, default=1,
                        help="minimum unique colors in each emulator capture")
    parser.add_argument("--sha256", help="optional expected ROM SHA-256")
    args = parser.parse_args()

    if args.alignment_window < 0 or args.min_colors < 1:
        parser.error("alignment window must be nonnegative and minimum colors positive")
    frames = [int(frame) for frame in args.frames.split(",") if frame]
    if not frames or any(frame <= 0 for frame in frames) or len(set(frames)) != len(frames):
        parser.error("--frames must contain unique positive comma-separated integers")

    digest = hashlib.sha256(args.rom.read_bytes()).hexdigest()
    if args.sha256 and digest.lower() != args.sha256.lower():
        raise RuntimeError(f"ROM SHA-256 mismatch: got {digest}, expected {args.sha256}")

    captures_dir = args.output_dir / "captures"
    logs_dir = args.output_dir / "logs"
    captures_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)
    label = args.rom.stem
    for frame_count in frames:
        actual = captures_dir / f"{label}-{frame_count}.ppm"
        expected = captures_dir / f"mgba-{label}-{frame_count}.ppm"
        emulator_log = logs_dir / f"emulator-{frame_count}.log"
        reference_log = logs_dir / f"mgba-{frame_count}.log"
        with emulator_log.open("wb") as log:
            subprocess.run([str(args.emulator.resolve()), str(args.rom.resolve()), "--headless",
                            "--no-save", "--frames", str(frame_count), "--dump-frame", str(actual)],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        with reference_log.open("wb") as log:
            subprocess.run([str(args.reference.resolve()), str(args.rom.resolve()),
                            str(frame_count), str(expected), str(args.alignment_window)],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        actual_pixels = pixels(actual)
        candidates = [(expected, frame_count)] + [
            (Path(str(expected) + f".{candidate}.ppm"), candidate)
            for candidate in range(max(1, frame_count - args.alignment_window),
                                   frame_count + args.alignment_window + 1)
            if candidate != frame_count]
        comparisons = [(compare(actual_pixels, pixels(path), args.min_colors), candidate)
                       for path, candidate in candidates]
        (mean, close), aligned = min(comparisons, key=lambda item: item[0][0])
        if mean > args.max_mean_error or close < args.min_close_pixels:
            raise RuntimeError(
                f"{label} frame {frame_count} differs from mGBA: "
                f"mean={mean:.4f}, close={close:.2%}")
        print(f"{label} frame {frame_count} matches mGBA frame {aligned}: "
              f"mean 5-bit error {mean:.4f}; {close:.2%} within 2")

    # Keep the per-game folders and logs, but remove image data once verification
    # has confirmed the run. Failed comparisons retain captures for inspection.
    for capture in captures_dir.glob("*.ppm"):
        capture.unlink()


if __name__ == "__main__":
    main()
