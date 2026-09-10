#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/ui-constitution-u4b3
c++ -std=c++17 -I. tests/test_ui_constitution_u4b3_phrase_duration_edit.cpp -o build/ui-constitution-u4b3/test_phrase_duration_edit
build/ui-constitution-u4b3/test_phrase_duration_edit
python3 tests/test_ui_constitution_u4b3_phrase_duration_integration.py
