#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/ui-constitution-u4b4
c++ -std=c++17 -I. tests/test_ui_constitution_u4b4_phrase_delete_edit.cpp -o build/ui-constitution-u4b4/test_phrase_delete_edit
build/ui-constitution-u4b4/test_phrase_delete_edit
python3 tests/test_ui_constitution_u4b4_phrase_delete_integration.py
