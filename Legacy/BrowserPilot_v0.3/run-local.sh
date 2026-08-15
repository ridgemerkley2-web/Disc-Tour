#!/usr/bin/env bash
cd "$(dirname "$0")"
echo "Starting Pine Ridge Disc Golf Pilot at http://localhost:8765"
python3 -m http.server 8765
