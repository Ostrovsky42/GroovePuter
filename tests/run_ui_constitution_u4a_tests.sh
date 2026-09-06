#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/ui-constitution-u4a
c++ -std=c++17 -I. tests/test_ui_constitution_u4a_phrase_viewport.cpp -o build/ui-constitution-u4a/test_phrase_viewport
build/ui-constitution-u4a/test_phrase_viewport
python3 tests/test_ui_constitution_u4a_phrase_viewport_integration.py
