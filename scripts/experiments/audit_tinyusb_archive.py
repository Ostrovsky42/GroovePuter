#!/usr/bin/env python3
"""Create an intentionally non-linkable class-removal candidate outside the SDK.

This proves why archive deletion alone is insufficient; it is NOT firmware.
No stubs, binary patching, flashing, or writes to the installed SDK are performed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def run(*args):
    return subprocess.check_output([str(x) for x in args], text=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    packages = Path.home() / '.arduino15/packages/m5stack/tools'
    parser.add_argument('--sdk', type=Path, default=packages / 'esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3')
    parser.add_argument('--tools', type=Path, default=packages / 'esp-x32/2411/bin')
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.sdk / 'lib/libarduino_tinyusb.a'
    output = args.output.resolve()
    if output == args.sdk.resolve() or args.sdk.resolve() in output.parents:
        parser.error('output must be outside the installed SDK')
    output.mkdir(parents=True, exist_ok=True)
    candidate = output / 'candidate-no-msc-dfu-ncm.a'
    if candidate.exists():
        parser.error('candidate already exists; use a fresh output directory')
    ar = args.tools / 'xtensa-esp32s3-elf-ar'
    nm = args.tools / 'xtensa-esp32s3-elf-nm'
    before = digest(source)
    removed = ['msc_device.c.obj', 'dfu_device.c.obj', 'ncm_device.c.obj']
    members = run(ar, 't', source).splitlines()
    assert set(removed) <= set(members)
    shutil.copyfile(source, candidate)
    run(ar, 'd', candidate, *removed)
    remaining = run(ar, 't', candidate).splitlines()
    assert all(member not in remaining for member in removed)
    # Preserve CDC/MIDI, runtime DFU and the unchanged dispatch object verbatim.
    preserved = {}
    for member in ['cdc_device.c.obj', 'midi_device.c.obj', 'dfu_rt_device.c.obj', 'usbd.c.obj']:
        old = subprocess.check_output([str(ar), 'p', str(source), member])
        new = subprocess.check_output([str(ar), 'p', str(candidate), member])
        assert old == new
        preserved[member] = hashlib.sha256(new).hexdigest()
    usbd = output / 'usbd.c.obj'
    usbd.write_bytes(subprocess.check_output([str(ar), 'p', str(source), 'usbd.c.obj']))
    undefined = {line.split()[-1] for line in run(nm, '-u', usbd).splitlines() if line.split()}
    defined = {line.split()[-1] for line in run(nm, '-g', '--defined-only', candidate).splitlines() if len(line.split()) >= 3}
    missing = sorted(symbol for symbol in undefined - defined if symbol.startswith(('mscd_', 'dfu_moded_', 'netd_')))
    assert missing, 'expected unresolved class dispatch references'
    buffers = {}
    for line in run(nm, '-S', '--size-sort', args.elf).splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[3] in ['_mscd_epbuf', '_mscd_itf', '_dfu_epbuf', '_dfu_ctx', 'ncm_epbuf', 'ncm_interface']:
            buffers[fields[3]] = int(fields[1], 16)
    assert len(buffers) == 6
    assert digest(source) == before
    report = dict(source=str(source), source_sha256=before, candidate_sha256=digest(candidate),
                  elf=str(args.elf.resolve()), elf_sha256=digest(args.elf), elf_bss_symbols= buffers,
                  removed_members=removed, preserved_member_sha256=preserved,
                  unresolved_dispatch_references=missing, recovered_bytes=None,
                  status='OPEN: candidate cannot link without rebuilding dispatch/config; no candidate ELF')
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
