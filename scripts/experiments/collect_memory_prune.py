#!/usr/bin/env python3
"""Collect ELF sections and actual serial PERF samples; missing runtime data stays OPEN."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pairs(values):
    result = {}
    for value in values:
        key, path = value.split('=', 1)
        result[key] = Path(path)
    return result


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--build', action='append', default=[], metavar='LABEL=DIR')
    p.add_argument('--monitor', action='append', default=[], metavar='LABEL=LOG')
    p.add_argument('--build-log', action='append', default=[], metavar='LABEL=LOG')
    p.add_argument('--output', required=True)
    args = p.parse_args()
    tool = os.environ.get('XTENSA_SIZE') or shutil.which('xtensa-esp-elf-size')
    if not tool:
        matches = sorted((Path.home()/'.arduino15/packages/m5stack/tools/esp-x32').glob('*/bin/xtensa-esp-elf-size'))
        if not matches:
            p.error('set XTENSA_SIZE to the matching target size tool')
        tool = str(matches[-1])
    nm = str(Path(tool).with_name('xtensa-esp-elf-nm'))
    monitors, logs = pairs(args.monitor), pairs(args.build_log)
    records = {}
    for label, build in pairs(args.build).items():
        elf, binary = build/'GroovePuter.ino.elf', build/'GroovePuter.ino.bin'
        output = subprocess.check_output([tool, '-A', str(elf)], text=True)
        sections = {m[1]: int(m[2]) for line in output.splitlines()
                    if (m := re.match(r'^(\.\S+)\s+(\d+)\s+', line))}
        symbols = subprocess.check_output([nm, '-C', '--defined-only', str(elf)], text=True)
        removed = ('SampleIndex::', 'RamSampleStore::', 'SamplerPool::', 'SamplerVoice::',
                   'DrumSamplerTrack::', 'SamplerPage::', 'TapeFX::', 'TapeLooper::')
        record = dict(build=str(build), elf_sha256=sha(elf), bin_sha256=sha(binary),
                      image_bytes=binary.stat().st_size,
                      data=sections['.dram0.data'], bss=sections['.dram0.bss'],
                      sections=sections,
                      optional_runtime_symbols=[s for s in removed if s in symbols])
        record['fixed_dram'] = record['data'] + record['bss']
        options = build/'.arduino-build/build.options.json'
        if options.exists():
            props = json.loads(options.read_text()).get('customBuildProperties', '')
            record['feature_flags'] = re.findall(r'-DGROOVEPUTER_\w+(?:=[^ ,]+)?', props)
        sources = build/'.arduino-build/sketch'
        tree = hashlib.sha256()
        if sources.exists():
            for f in sorted(sources.rglob('*')):
                if f.is_file() and f.suffix in ('.cpp', '.h', '.ino'):
                    content = '\n'.join(l for l in f.read_text(errors='replace').splitlines()
                                        if not l.startswith('#line '))
                    tree.update(str(f.relative_to(sources)).encode()+b'\0'+content.encode()+b'\0')
            record['normalized_staged_source_sha256'] = tree.hexdigest()
        if label in logs:
            text = logs[label].read_text(errors='replace')
            m = re.search(r'Sketch uses (\d+) bytes', text)
            record['arduino_flash_bytes'] = int(m[1]) if m else None
        record['runtime'] = 'OPEN — no hardware serial capture for this image'
        if label in monitors:
            text = monitors[label].read_text(errors='replace')
            samples = []
            for line in text.splitlines():
                m = re.search(r'underruns=(\d+).*freeInt=(\d+) largest=(\d+)', line)
                if '[PERF]' in line and m:
                    samples.append(dict(free=int(m[2]), largest=int(m[3]), underruns=int(m[1])))
            record['runtime'] = dict(log=str(monitors[label]), log_sha256=sha(monitors[label]),
                samples=samples, last=samples[-1] if samples else None,
                free_min=min((x['free'] for x in samples), default=None),
                largest_min=min((x['largest'] for x in samples), default=None),
                fatal_markers=re.findall(r'Guru Meditation|CORRUPT HEAP|Backtrace:', text),
                key_presses=len(re.findall(r'\[KEY\]', text)))
        records[label] = record
    Path(args.output).write_text(json.dumps(dict(size_tool=tool, images=records), indent=2)+'\n')


if __name__ == '__main__':
    main()
