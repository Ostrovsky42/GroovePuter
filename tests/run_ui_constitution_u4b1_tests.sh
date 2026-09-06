#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/ui-constitution-u4b1
c++ -std=c++17 -I. tests/test_ui_constitution_u4b1_phrase_lane_layout.cpp -o build/ui-constitution-u4b1/test_phrase_lane_layout
build/ui-constitution-u4b1/test_phrase_lane_layout
python3 tests/test_ui_constitution_u4b1_phrase_lane_layout_integration.py
