#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/sampler-wav-loader"
CORPUS_DIR="${BUILD_DIR}/corpus"
CXX="${CXX:-g++}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
python3 "${ROOT_DIR}/tests/generate_sampler_wav_corpus.py" "${CORPUS_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_wav_loader.cpp" \
  "${ROOT_DIR}/src/sampler/sample_loader.cpp" \
  "${ROOT_DIR}/src/sampler/ram_sample_store.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  -o "${BUILD_DIR}/test_sampler_wav_loader"

"${BUILD_DIR}/test_sampler_wav_loader" "${CORPUS_DIR}"
