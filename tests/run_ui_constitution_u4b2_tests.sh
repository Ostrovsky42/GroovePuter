#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/ui-constitution-u4b2
c++ -std=c++17 -I. tests/test_ui_constitution_u4b2_phrase_cursor_selection.cpp -o build/ui-constitution-u4b2/test_phrase_cursor_selection
build/ui-constitution-u4b2/test_phrase_cursor_selection
python3 tests/test_ui_constitution_u4b2_phrase_cursor_integration.py
