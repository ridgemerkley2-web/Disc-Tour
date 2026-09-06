#!/usr/bin/env python3
"""Generate the original, deterministic Session 19 presentation-audio candidate pack."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import random
import struct
import wave


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = ROOT / "OriginalAudio/Session19Generated"
MANIFEST_PATH = ROOT / "Config/DG_Session19OriginalAudioCandidatePack.json"
SAMPLE_RATE = 48_000

SPECS = [
    ("ThrowRelease", "DG_ThrowRelease.wav", 0.55, 19001),
    ("AirborneFlight", "DG_AirborneFlight.wav", 1.60, 19002),
    ("GroundContact", "DG_GroundContact.wav", 0.65, 19003),
    ("GroundState", "DG_GroundState.wav", 1.20, 19004),
    ("BasketOutcome", "DG_BasketOutcome.wav", 1.05, 19005),
    ("Penalty", "DG_Penalty.wav", 0.90, 19006),
    ("HoleStart", "DG_HoleStart.wav", 1.15, 19007),
    ("HoleCompletion", "DG_HoleCompletion.wav", 1.35, 19008),
    ("HoleTransition", "DG_HoleTransition.wav", 1.10, 19009),
    ("RoundCompletion", "DG_RoundCompletion.wav", 1.70, 19010),
    ("Replay", "DG_Replay.wav", 0.80, 19011),
    ("Flyover", "DG_Flyover.wav", 1.80, 19012),
]


def _clamp(value: float) -> float:
    return max(-1.0, min(1.0, value))


def _tone(time_s: float, frequency: float, phase: float = 0.0) -> float:
    return math.sin(2.0 * math.pi * frequency * time_s + phase)


def _synthesize(category: str, duration: float, seed: int) -> list[float]:
    count = round(duration * SAMPLE_RATE)
    rng = random.Random(seed)
    samples: list[float] = []
    filtered_noise = 0.0
    for index in range(count):
        time_s = index / SAMPLE_RATE
        progress = index / max(1, count - 1)
        attack = min(1.0, progress / 0.025)
        release = min(1.0, (1.0 - progress) / 0.10)
        envelope = attack * release
        white = rng.uniform(-1.0, 1.0)
        filtered_noise = filtered_noise * 0.92 + white * 0.08

        if category == "ThrowRelease":
            value = (0.55 * filtered_noise + 0.35 * _tone(time_s, 180 + 520 * progress)) \
                * math.exp(-4.2 * progress)
        elif category == "AirborneFlight":
            loop_envelope = 0.72 + 0.28 * math.sin(math.pi * progress)
            value = loop_envelope * (0.42 * filtered_noise + 0.16 * _tone(time_s, 126)
                                     + 0.10 * _tone(time_s, 252, 0.7))
        elif category == "GroundContact":
            value = (0.65 * _tone(time_s, 74) + 0.40 * filtered_noise) * math.exp(-8.0 * progress)
        elif category == "GroundState":
            pulses = 0.5 + 0.5 * math.sin(2.0 * math.pi * 9.0 * time_s)
            value = (0.34 * filtered_noise + 0.18 * _tone(time_s, 102)) * pulses * (1.0 - 0.55 * progress)
        elif category == "BasketOutcome":
            value = sum(
                0.22 * _tone(time_s, frequency, phase) * math.exp(-decay * progress)
                for frequency, phase, decay in ((610, 0.0, 3.2), (947, 0.4, 4.0), (1327, 1.1, 5.0))
            )
        elif category == "Penalty":
            second = max(0.0, progress - 0.42)
            value = 0.40 * _tone(time_s, 146) * math.exp(-5.0 * progress)
            if second > 0.0:
                value += 0.34 * _tone(time_s, 116) * math.exp(-8.0 * second)
        elif category == "HoleStart":
            value = sum(
                0.24 * _tone(time_s, frequency) * math.exp(-3.2 * max(0.0, progress - offset))
                * (1.0 if progress >= offset else 0.0)
                for frequency, offset in ((392, 0.0), (494, 0.18), (587, 0.36))
            )
        elif category == "HoleCompletion":
            value = sum(0.19 * _tone(time_s, frequency) for frequency in (330, 415, 494)) \
                * math.exp(-2.4 * progress)
        elif category == "HoleTransition":
            frequency = 210 + 390 * progress * progress
            value = 0.42 * _tone(time_s, frequency) * math.sin(math.pi * progress)
        elif category == "RoundCompletion":
            chord = sum(0.17 * _tone(time_s, frequency) for frequency in (262, 330, 392, 523))
            value = chord * (0.55 + 0.45 * math.sin(math.pi * progress))
        elif category == "Replay":
            frequency = 820 - 510 * progress
            value = (0.34 * _tone(time_s, frequency) + 0.14 * filtered_noise) * math.sin(math.pi * progress)
        elif category == "Flyover":
            value = (0.22 * _tone(time_s, 196) + 0.15 * _tone(time_s, 294, 0.4)
                     + 0.12 * filtered_noise) * math.sin(math.pi * progress)
        else:
            raise ValueError(f"unknown category: {category}")
        samples.append(_clamp(value * envelope))

    peak = max(abs(value) for value in samples) or 1.0
    gain = 0.78 / peak
    return [_clamp(value * gain) for value in samples]


def _wav_bytes(samples: list[float]) -> bytes:
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(b"".join(
            struct.pack("<h", round(_clamp(value) * 32767.0)) for value in samples
        ))
    return output.getvalue()


def _build() -> tuple[dict[str, bytes], dict[str, object]]:
    files: dict[str, bytes] = {}
    records: list[dict[str, object]] = []
    for category, filename, duration, seed in SPECS:
        data = _wav_bytes(_synthesize(category, duration, seed))
        files[filename] = data
        records.append({
            "category": category,
            "sourcePath": f"OriginalAudio/Session19Generated/{filename}",
            "unrealPackage": f"/Game/Presentation/Audio/Generated/SW_{category}",
            "unrealObject": f"/Game/Presentation/Audio/Generated/SW_{category}.SW_{category}",
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest().upper(),
            "sampleRateHz": SAMPLE_RATE,
            "channels": 1,
            "sampleWidthBits": 16,
            "frameCount": round(duration * SAMPLE_RATE),
            "durationSeconds": duration,
            "seed": seed,
            "rights": "PROJECT_OWNED_ORIGINAL_PROCEDURAL_SYNTHESIS",
            "externalSamplesUsed": False,
            "humanMixApproved": False,
            "finalQualityApproved": False,
        })
    generator_data = Path(__file__).read_bytes()
    manifest: dict[str, object] = {
        "schema": "DiscGolfTour.Session19OriginalAudioCandidatePack.v1",
        "schemaVersion": 1,
        "packId": "dg_session19_original_audio_candidate_v1",
        "generator": {
            "path": "Scripts/generate_dg_session19_audio_candidate_pack.py",
            "bytes": len(generator_data),
            "sha256": hashlib.sha256(generator_data).hexdigest().upper(),
            "deterministic": True,
            "externalDependencies": [],
        },
        "provenance": {
            "author": "Disc Golf Tour project",
            "method": "Original mathematical oscillators, deterministic seeded noise, and envelopes",
            "externalDownloads": False,
            "externalRecordingsOrSamples": False,
            "thirdPartyGenerativeService": False,
            "projectOwnedCandidateClaim": True,
        },
        "audioFormat": {"container": "RIFF_WAVE", "encoding": "PCM_S16LE"},
        "categoryCount": len(records),
        "categories": records,
        "claimBoundary": {
            "sourceMastersGeneratedAndHashBound": True,
            "runtimeHooksCovered": True,
            "unrealAssetsImported": False,
            "shippingCookProven": False,
            "humanMixApproved": False,
            "finalQualityApproved": False,
            "accessibilityApproved": False,
            "legalApproved": False,
            "productOwnerApproved": False,
            "blockerClosed": False,
            "releaseReady": False,
        },
    }
    return files, manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if args.write == args.verify:
        parser.error("choose exactly one of --write or --verify")

    files, manifest = _build()
    manifest_bytes = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    if args.write:
        OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
        for filename, data in files.items():
            (OUTPUT_ROOT / filename).write_bytes(data)
        MANIFEST_PATH.write_bytes(manifest_bytes)
        print(f"WROTE_ORIGINAL_AUDIO_CANDIDATE_PACK ({len(files)} WAV masters)")
        return 0

    failures: list[str] = []
    for filename, data in files.items():
        path = OUTPUT_ROOT / filename
        if not path.is_file() or path.read_bytes() != data:
            failures.append(filename)
    if not MANIFEST_PATH.is_file() or MANIFEST_PATH.read_bytes() != manifest_bytes:
        failures.append(MANIFEST_PATH.name)
    if failures:
        print("FAIL_ORIGINAL_AUDIO_CANDIDATE_PACK: " + ", ".join(failures))
        return 1
    print(f"PASS_ORIGINAL_AUDIO_CANDIDATE_PACK ({len(files)} WAV masters, byte-exact)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
