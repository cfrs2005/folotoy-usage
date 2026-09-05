#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mode="${1:---all}"
case "$mode" in --static|--firmware|--all) ;; *) exit 2 ;; esac
if [[ "$mode" != --firmware ]]; then
    python3 tools/audit_public.py
    python3 -m unittest discover -s tests -p 'test_*.py'
    : "${IDF_PATH:?Activate ESP-IDF 5.5.3 for the pinned cJSON host tests}"
    test_dir="$(mktemp -d)"
    trap 'rm -rf "$test_dir"' EXIT
    cc -std=c11 -Wall -Wextra -Werror -Imain -I"$IDF_PATH/components/json/cJSON" \
       tests/test_usage_model.c main/usage_model.c "$IDF_PATH/components/json/cJSON/cJSON.c" \
       -lm -o "$test_dir/model-test"
    "$test_dir/model-test"
fi
if [[ "$mode" != --static ]]; then
    idf.py build
    idf.py merge-bin -o FoloToy-AI-Passport-full.bin
    python3 tools/verify_firmware.py build
fi
