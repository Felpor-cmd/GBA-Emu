#!/usr/bin/env python3
"""Capture matched-rate audio and summarize timing without external packages."""
import argparse
import array
import csv
import json
import math
import pathlib
import statistics
import subprocess
import sys
import wave


def audio_metrics(path):
    with wave.open(str(path)) as wav:
        if (wav.getframerate(), wav.getnchannels(), wav.getsampwidth()) != (48000, 2, 2):
            raise ValueError(f"{path}: expected 48 kHz stereo PCM16")
        pcm = array.array("h", wav.readframes(wav.getnframes()))
        if sys.byteorder != "little":
            pcm.byteswap()
        seconds = wav.getnframes() / 48000
    # Ignore startup latency and remove DC before measuring tone frequency.
    left = pcm[24000::2]
    mean = statistics.fmean(left) if left else 0
    crossings = [i for i in range(1, len(left)) if left[i - 1] < mean <= left[i]]
    hz = ((len(crossings) - 1) * 48000 / (crossings[-1] - crossings[0])) if len(crossings) > 1 else 0
    return {"seconds": seconds, "dc": mean,
            "rms_without_dc": math.sqrt(statistics.fmean((x - mean) ** 2 for x in left)) if left else 0,
            "rising_crossing_hz": hz,
            "peak": max(map(abs, pcm), default=0)}


def frame_metrics(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    timings = [float(row["frame_ms"]) for row in rows]
    budget = 280896 / 16777216 * 1000
    return {"mean_ms": statistics.fmean(timings), "p95_ms": sorted(timings)[int(len(timings) * .95)],
            "mean_hardware_ms": statistics.fmean(float(row.get("hardware_ms") or 0) for row in rows),
            "mean_render_ms": statistics.fmean(float(row.get("render_ms") or 0) for row in rows),
            "frames_over_budget": sum(value > budget for value in timings),
            "bias_values": sorted({hex(int(row["bias"])) for row in rows})}


def fifo_events(path):
    with path.open() as stream:
        return [(int(r["cycle"]), int(r["index"]), int(r["value"]) & 255)
                for r in csv.DictReader(stream) if int(r["kind"]) == 2]


def event_comparison(directory):
    ours_path, ref_path = (directory / "logs" / f"{name}-events.csv" for name in ("ours", "mgba"))
    if not (ours_path.exists() and ref_path.exists()):
        return None
    # Compare each FIFO separately; ignore events from an unrouted channel.
    ours, reference = fifo_events(ours_path), fifo_events(ref_path)
    result = {}
    for fifo in (0, 1):
        a = [(t, v) for t, i, v in ours if i == fifo]
        b = [(t, v) for t, i, v in reference if i == fifo]
        if not b:
            continue
        mismatch = next((i for i, (x, y) in enumerate(zip(a, b)) if x[1] != y[1]), None)
        offset = a[0][0] - b[0][0] if a and b else None
        drift = next((i for i, (x, y) in enumerate(zip(a, b)) if x[0] - y[0] != offset), None)
        result[str(fifo)] = {"ours_events": len(a), "reference_events": len(b),
                             "first_byte_mismatch": mismatch, "first_timing_drift": drift,
                             "fixed_cycle_offset": offset}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", default="build/audio_diagnostics")
    parser.add_argument("--rom", default="roms/Zelda.gba", help="ROM or @square/@wave/@noise/@fifo/@mixed")
    parser.add_argument("--frames", type=int, default=900)
    parser.add_argument("--start", type=int, default=180)
    parser.add_argument("--output-dir", type=pathlib.Path)
    parser.add_argument("--compare-only", action="store_true")
    parser.add_argument("--events", action="store_true", help="Trace events; excludes performance conclusions")
    parser.add_argument("--require-realtime", action="store_true", help="Require local mean frame time below the playback budget")
    parser.add_argument("--check-playback", type=pathlib.Path, help="Check an SDL --audio-diagnostics CSV instead of capturing audio")
    args = parser.parse_args()
    if args.check_playback:
        with args.check_playback.open() as stream:
            rows = list(csv.DictReader(stream))
        result = {"underrun_samples": max(int(r["underrun_samples"]) for r in rows),
                  "dropped_samples": max(int(r["dropped_samples"]) for r in rows),
                  "output_frames": max(int(r["output_frames"]) for r in rows)}
        passed = result["underrun_samples"] == 0 and result["dropped_samples"] == 0 and result["output_frames"] > 0
        result["passed"] = passed
        args.check_playback.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
        return 0 if passed else 1
    directory = args.output_dir or pathlib.Path("/tmp/gba-emu") / pathlib.Path(args.rom).stem.lstrip("@")
    if not args.compare_only:
        for name in ("ours", "mgba"):
            command = [args.runner, args.rom, str(args.frames), str(directory), "--start", str(args.start)]
            if name == "mgba":
                command.append("--reference")
            if args.events:
                command.append("--events")
            output = subprocess.run(command, check=True, text=True, capture_output=True)
            (directory / "logs" / f"{name}-run.log").write_text(output.stdout + output.stderr)
    report = {"sample_rate": 48000, "performance_trace_enabled": args.events}
    for name in ("ours", "mgba"):
        report[name] = {"audio": audio_metrics(directory / "captures" / f"{name}.wav"),
                        "frames": frame_metrics(directory / "logs" / f"{name}.csv")}
    report["fifo_comparison"] = event_comparison(directory)
    duration_delta = abs(report["ours"]["audio"]["seconds"] - report["mgba"]["audio"]["seconds"])
    # mGBA's skip-BIOS video reset starts partway through a frame. Allow that
    # fixed startup offset plus filter latency, without stretching recordings.
    passed = duration_delta < .05
    expected_hz = {"@square": 256, "@wave": 256, "@fifo": 64}.get(args.rom)
    if expected_hz:
        passed &= all(abs(report[name]["audio"]["rising_crossing_hz"] / expected_hz - 1) < .01
                      for name in ("ours", "mgba"))
    if args.rom == "@fifo" and args.events:
        comparison = report["fifo_comparison"] or {}
        passed &= bool(comparison) and all(row["first_byte_mismatch"] is None and
            row["first_timing_drift"] is None for row in comparison.values())
    report["duration_and_tone_checks_passed"] = passed
    if args.require_realtime:
        if args.events:
            parser.error("--require-realtime cannot be used with event tracing")
        report["real_time_check_passed"] = report["ours"]["frames"]["mean_ms"] < 280896 / 16777216 * 1000
        passed &= report["real_time_check_passed"]
    text = json.dumps(report, indent=2)
    (directory / "logs" / "comparison.json").write_text(text + "\n")
    print(text)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
