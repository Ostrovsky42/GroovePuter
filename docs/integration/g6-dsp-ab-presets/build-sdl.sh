#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
python3 - "$ROOT" <<'PY'
import os
from pathlib import Path
import subprocess
import sys
root = Path(sys.argv[1])
build = root / 'build/integration-sdl'
shim = root / 'build/dual-osc-preset-ui/felucca_analog_c.o'
build.mkdir(parents=True, exist_ok=True)
shim.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O1', '-I' + str(root),
                '-c', str(root / 'src/dsp/felucca_analog_c.c'), '-o', str(shim)], check=True)
listing = subprocess.run(['make', '-C', str(root / 'platform_sdl'), '-pn'],
                         capture_output=True, text=True, check=True).stdout
sources = next(line[len('SOURCES := '):] for line in listing.splitlines()
               if line.startswith('SOURCES := '))
sources += ' ../src/dsp/felucca_analog_voice.cpp ../src/sampler/sample_scene_persistence.cpp ' + str(shim)
flags = ('-std=c++17 -Wno-c++20-extensions -O1 -I.. -I. -include arduino_compat.h '
         '-DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_FELUCCA_ANALOG=1 '
         '-DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0')
subprocess.run(['make', '-C', str(root / 'platform_sdl'), '-j2',
                'CXX=' + os.environ.get('CXX', 'g++'), 'TARGET=' + str(build / 'grooveputer'),
                'SOURCES=' + sources, 'CXXFLAGS=' + flags], check=True)
PY
