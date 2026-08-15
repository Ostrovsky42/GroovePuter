#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path


def le16(v): return struct.pack('<H', v)
def le32(v): return struct.pack('<I', v)


def chunk(tag: bytes, payload: bytes, pad: bool = True) -> bytes:
    assert len(tag) == 4
    out = tag + le32(len(payload)) + payload
    if pad and (len(payload) & 1):
        out += b'\x00'
    return out


def fmt_payload(audio_format=1, channels=1, rate=22050, bits=16,
                block_align=None, byte_rate=None, extra=b''):
    if block_align is None:
        block_align = channels * (bits // 8)
    if byte_rate is None:
        byte_rate = rate * block_align
    return (le16(audio_format) + le16(channels) + le32(rate) + le32(byte_rate) +
            le16(block_align) + le16(bits) + extra)


def pcm16(values):
    return b''.join(struct.pack('<h', v) for v in values)


def riff(chunks, declared_size=None):
    body = b'WAVE' + b''.join(chunks)
    size = len(body) if declared_size is None else declared_size
    return b'RIFF' + le32(size) + body


def write(path: Path, data: bytes):
    path.write_bytes(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)

    mono = pcm16([1000, -1000, 32767, -32768])
    stereo = pcm16([1000, 3000, -1000, 1000, 32767, 32767, -32768, -32768])
    stereo_multichunk_values = []
    for i in range(300):
        left = (i * 97) % 30000 - 15000
        right = 15000 - ((i * 53) % 30000)
        stereo_multichunk_values.extend((left, right))
    stereo_multichunk = pcm16(stereo_multichunk_values)

    canonical = [chunk(b'fmt ', fmt_payload()), chunk(b'data', mono)]
    write(out/'valid_mono.wav', riff(canonical))
    write(out/'valid_stereo.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=2)), chunk(b'data', stereo)]))
    write(out/'valid_stereo_multichunk.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=2)), chunk(b'data', stereo_multichunk)]))
    write(out/'odd_junk.wav', riff([
        chunk(b'JUNK', b'X'), chunk(b'fmt ', fmt_payload()), chunk(b'data', mono)]))
    write(out/'odd_list.wav', riff([
        chunk(b'fmt ', fmt_payload()), chunk(b'LIST', b'abc'), chunk(b'data', mono)]))
    write(out/'fmt_extension.wav', riff([
        chunk(b'fmt ', fmt_payload(extra=b'\x00\x00')), chunk(b'data', mono)]))
    write(out/'data_before_fmt.wav', riff([
        chunk(b'data', mono), chunk(b'JUNK', b'abc'), chunk(b'fmt ', fmt_payload())]))
    write(out/'valid_unknown_after_data.wav', riff([
        chunk(b'fmt ', fmt_payload()), chunk(b'data', mono), chunk(b'fact', b'xyz')]))
    write(out/'physical_trailing_bytes.wav', riff(canonical) + b'TRAILING-NON-RIFF-DATA')

    write(out/'invalid_riff.wav', b'NOPE' + le32(4) + b'WAVE')
    write(out/'riff_too_short.wav', b'RIFF' + le32(3) + b'WAVE')

    valid = riff(canonical)
    write(out/'truncated_file.wav', valid[:-2])

    body = b'WAVE' + b''.join(canonical)
    write(out/'truncated_riff.wav', b'RIFF' + le32(len(body) + 64) + body)

    huge_header = b'JUNK' + le32(0xFFFFFFFF)
    write(out/'oversized_chunk.wav', riff([huge_header], declared_size=4 + len(huge_header)))

    missing_pad_body = (b'WAVE' + b'JUNK' + le32(1) + b'X' +
                        chunk(b'fmt ', fmt_payload()) + chunk(b'data', mono))
    write(out/'missing_odd_pad.wav', b'RIFF' + le32(len(missing_pad_body)) + missing_pad_body)

    write(out/'fmt_too_short.wav', riff([
        chunk(b'fmt ', b'\x01\x00\x01\x00'), chunk(b'data', mono)]))
    write(out/'missing_fmt.wav', riff([chunk(b'JUNK', b'ab'), chunk(b'data', mono)]))
    write(out/'missing_data.wav', riff([chunk(b'fmt ', fmt_payload())]))
    write(out/'float32.wav', riff([
        chunk(b'fmt ', fmt_payload(audio_format=3, bits=32)), chunk(b'data', b'\x00'*16)]))
    write(out/'pcm8.wav', riff([
        chunk(b'fmt ', fmt_payload(bits=8, block_align=1, byte_rate=22050)),
        chunk(b'data', b'\x80\x81\x82\x83')]))
    write(out/'pcm24.wav', riff([
        chunk(b'fmt ', fmt_payload(bits=24)), chunk(b'data', b'\x00'*12)]))
    write(out/'zero_channels.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=0, block_align=0)), chunk(b'data', mono)]))
    write(out/'three_channels.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=3)), chunk(b'data', b'\x00'*12)]))
    write(out/'zero_rate.wav', riff([
        chunk(b'fmt ', fmt_payload(rate=0)), chunk(b'data', mono)]))
    write(out/'bad_block_align.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=2, block_align=2)), chunk(b'data', stereo)]))
    write(out/'bad_byte_rate.wav', riff([
        chunk(b'fmt ', fmt_payload(byte_rate=1234)), chunk(b'data', mono)]))
    write(out/'unaligned_data.wav', riff([
        chunk(b'fmt ', fmt_payload(channels=2)), chunk(b'data', stereo + b'\x00\x01')]))
    write(out/'empty_data.wav', riff([
        chunk(b'fmt ', fmt_payload()), chunk(b'data', b'')]))
    write(out/'duplicate_fmt.wav', riff([
        chunk(b'fmt ', fmt_payload()), chunk(b'fmt ', fmt_payload()), chunk(b'data', mono)]))
    write(out/'duplicate_data.wav', riff([
        chunk(b'fmt ', fmt_payload()), chunk(b'data', mono), chunk(b'data', mono)]))

    trailing_body = (b'WAVE' + b''.join(canonical) +
                     b'JUNK' + le32(4) + b'\x00\x01')
    write(out/'malformed_trailing_chunk.wav',
          b'RIFF' + le32(len(trailing_body)) + trailing_body)

    write(out/'changed_after_inspect.wav', riff(canonical))

    print(f'generated {len(list(out.glob("*.wav")))} WAV fixtures in {out}')


if __name__ == '__main__':
    main()
