#!/usr/bin/env python3
import struct
import unittest
from dataclasses import dataclass

UINT32_MAX = 0xFFFFFFFF


class WavContractError(ValueError):
    pass


@dataclass(frozen=True)
class WavProbe:
    sample_rate: int
    source_channels: int
    bits_per_sample: int
    block_align: int
    data_offset: int
    source_data_bytes: int
    frame_count: int
    decoded_mono_bytes: int


def make_chunk(chunk_id: bytes, payload: bytes, *, add_padding: bool = True) -> bytes:
    if len(chunk_id) != 4:
        raise ValueError("chunk id must be exactly four bytes")
    result = chunk_id + struct.pack("<I", len(payload)) + payload
    if add_padding and (len(payload) & 1):
        result += b"\x00"
    return result


def make_fmt(*, audio_format: int = 1, channels: int = 1,
             sample_rate: int = 22050, bits: int = 16,
             block_align: int | None = None,
             byte_rate: int | None = None,
             extra: bytes = b"") -> bytes:
    if block_align is None:
        block_align = channels * (bits // 8)
    if byte_rate is None:
        byte_rate = sample_rate * block_align
    payload = struct.pack(
        "<HHIIHH",
        audio_format,
        channels,
        sample_rate,
        byte_rate & UINT32_MAX,
        block_align,
        bits,
    ) + extra
    return make_chunk(b"fmt ", payload)


def make_riff(chunks: list[bytes], *, riff_size_adjust: int = 0,
              trailing: bytes = b"") -> bytes:
    body = b"WAVE" + b"".join(chunks)
    riff_size = len(body) + riff_size_adjust
    if riff_size < 0 or riff_size > UINT32_MAX:
        raise ValueError("invalid RIFF size")
    return b"RIFF" + struct.pack("<I", riff_size) + body + trailing


def inspect_wav(blob: bytes) -> WavProbe:
    if len(blob) < 12:
        raise WavContractError("truncated RIFF header")
    if blob[0:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise WavContractError("not RIFF/WAVE")

    riff_size = struct.unpack_from("<I", blob, 4)[0]
    riff_end = 8 + riff_size
    if riff_end < 12 or riff_end > len(blob):
        raise WavContractError("declared RIFF boundary exceeds physical file")

    fmt = None
    data = None
    cursor = 12

    while cursor < riff_end:
        if riff_end - cursor < 8:
            raise WavContractError("truncated chunk header")

        chunk_id = blob[cursor:cursor + 4]
        chunk_size = struct.unpack_from("<I", blob, cursor + 4)[0]
        payload_start = cursor + 8
        payload_end = payload_start + chunk_size
        if payload_end > riff_end:
            raise WavContractError("chunk payload exceeds RIFF boundary")

        next_chunk = payload_end + (chunk_size & 1)
        if next_chunk > riff_end:
            raise WavContractError("missing RIFF padding byte")

        if chunk_id == b"fmt ":
            if fmt is not None:
                raise WavContractError("duplicate fmt chunk")
            if chunk_size < 16:
                raise WavContractError("fmt chunk shorter than PCM core")
            fmt = struct.unpack_from("<HHIIHH", blob, payload_start)
        elif chunk_id == b"data":
            if data is not None:
                raise WavContractError("duplicate data chunk")
            data = (payload_start, chunk_size)

        cursor = next_chunk

    if cursor != riff_end:
        raise WavContractError("RIFF traversal did not end at declared boundary")
    if fmt is None:
        raise WavContractError("missing fmt chunk")
    if data is None:
        raise WavContractError("missing data chunk")

    audio_format, channels, sample_rate, byte_rate, block_align, bits = fmt
    if audio_format != 1:
        raise WavContractError("PCM format tag 1 required")
    if channels not in (1, 2):
        raise WavContractError("only mono/stereo accepted")
    if bits != 16:
        raise WavContractError("only PCM16 accepted")
    if sample_rate <= 0:
        raise WavContractError("sample rate must be positive")

    expected_block_align = channels * 2
    if block_align != expected_block_align:
        raise WavContractError("invalid block align")

    expected_byte_rate = sample_rate * block_align
    if expected_byte_rate > UINT32_MAX or byte_rate != expected_byte_rate:
        raise WavContractError("invalid byte rate")

    data_offset, data_size = data
    if data_size == 0 or data_size % block_align != 0:
        raise WavContractError("data size is not complete PCM frames")

    frame_count = data_size // block_align
    if frame_count == 0:
        raise WavContractError("empty PCM payload")

    decoded_bytes = frame_count * 2
    if decoded_bytes > UINT32_MAX:
        raise WavContractError("decoded byte count exceeds 32-bit size_t contract")

    return WavProbe(
        sample_rate=sample_rate,
        source_channels=channels,
        bits_per_sample=bits,
        block_align=block_align,
        data_offset=data_offset,
        source_data_bytes=data_size,
        frame_count=frame_count,
        decoded_mono_bytes=decoded_bytes,
    )


def admit_decoded_sample(probe: WavProbe, *, pool_free_bytes: int,
                         physical_free_bytes: int,
                         largest_free_block: int) -> bool:
    required = probe.decoded_mono_bytes
    return (
        required <= pool_free_bytes
        and required <= physical_free_bytes
        and required <= largest_free_block
    )


def decode_stereo_to_mono(blob: bytes, probe: WavProbe,
                          scratch_bytes: int) -> bytes:
    if probe.source_channels != 2:
        raise ValueError("stereo probe required")
    if scratch_bytes < 4:
        raise ValueError("scratch buffer must hold one stereo PCM16 frame")

    chunk_bytes = scratch_bytes - (scratch_bytes % 4)
    source = memoryview(blob)[
        probe.data_offset:probe.data_offset + probe.source_data_bytes
    ]
    output = bytearray()

    for offset in range(0, len(source), chunk_bytes):
        chunk = source[offset:offset + chunk_bytes]
        if len(chunk) % 4 != 0:
            raise WavContractError("decoder received a partial stereo frame")
        for frame in range(0, len(chunk), 4):
            left, right = struct.unpack_from("<hh", chunk, frame)
            mono = int((int(left) + int(right)) / 2)
            output += struct.pack("<h", mono)

    if len(output) != probe.decoded_mono_bytes:
        raise WavContractError("decoded output size mismatch")
    return bytes(output)


class WavLoader095ResearchContract(unittest.TestCase):
    def test_accepts_pcm16_mono(self) -> None:
        pcm = struct.pack("<hhhh", -32768, -1, 1, 32767)
        wav = make_riff([make_fmt(channels=1), make_chunk(b"data", pcm)])
        probe = inspect_wav(wav)
        self.assertEqual(probe.source_channels, 1)
        self.assertEqual(probe.frame_count, 4)
        self.assertEqual(probe.source_data_bytes, 8)
        self.assertEqual(probe.decoded_mono_bytes, 8)

    def test_accepts_pcm16_stereo_and_reports_decoded_not_source_bytes(self) -> None:
        pcm = struct.pack("<hhhhhhhh", 100, -100, 200, 100, -300, -100, 32767, 32767)
        wav = make_riff([make_fmt(channels=2), make_chunk(b"data", pcm)])
        probe = inspect_wav(wav)
        self.assertEqual(probe.frame_count, 4)
        self.assertEqual(probe.source_data_bytes, 16)
        self.assertEqual(probe.decoded_mono_bytes, 8)

    def test_accepts_data_before_fmt(self) -> None:
        pcm = struct.pack("<hh", 10, 20)
        wav = make_riff([make_chunk(b"data", pcm), make_fmt(channels=1)])
        self.assertEqual(inspect_wav(wav).frame_count, 2)

    def test_traverses_unknown_even_and_odd_chunks_with_padding(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        wav = make_riff([
            make_chunk(b"JUNK", b"abc"),
            make_chunk(b"LIST", b"abcd"),
            make_fmt(channels=1, extra=b"\x00\x00"),
            make_chunk(b"data", pcm),
            make_chunk(b"fact", b"xyz"),
        ])
        self.assertEqual(inspect_wav(wav).decoded_mono_bytes, 4)

    def test_allows_bytes_after_declared_riff_boundary(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        wav = make_riff(
            [make_fmt(channels=1), make_chunk(b"data", pcm)],
            trailing=b"TRAILING-NON-RIFF-DATA",
        )
        self.assertEqual(inspect_wav(wav).frame_count, 2)

    def test_rejects_truncated_riff_header(self) -> None:
        with self.assertRaises(WavContractError):
            inspect_wav(b"RIFF\x00\x00")

    def test_rejects_non_wave_riff(self) -> None:
        blob = b"RIFF" + struct.pack("<I", 4) + b"AVI "
        with self.assertRaises(WavContractError):
            inspect_wav(blob)

    def test_rejects_declared_riff_larger_than_file(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        wav = make_riff(
            [make_fmt(channels=1), make_chunk(b"data", pcm)],
            riff_size_adjust=8,
        )
        with self.assertRaises(WavContractError):
            inspect_wav(wav)

    def test_rejects_truncated_chunk_header(self) -> None:
        body = b"WAVE" + b"JUN"
        wav = b"RIFF" + struct.pack("<I", len(body)) + body
        with self.assertRaises(WavContractError):
            inspect_wav(wav)

    def test_rejects_chunk_payload_past_riff_boundary(self) -> None:
        bad = b"JUNK" + struct.pack("<I", 100) + b"short"
        wav = make_riff([bad])
        with self.assertRaises(WavContractError):
            inspect_wav(wav)

    def test_rejects_missing_odd_chunk_padding(self) -> None:
        odd_without_pad = make_chunk(b"JUNK", b"abc", add_padding=False)
        pcm = struct.pack("<hh", 1, 2)
        wav = make_riff([odd_without_pad, make_fmt(channels=1), make_chunk(b"data", pcm)])
        with self.assertRaises(WavContractError):
            inspect_wav(wav)

    def test_rejects_short_fmt(self) -> None:
        wav = make_riff([
            make_chunk(b"fmt ", b"\x01\x00\x01\x00"),
            make_chunk(b"data", struct.pack("<h", 1)),
        ])
        with self.assertRaises(WavContractError):
            inspect_wav(wav)

    def test_rejects_float_and_non_pcm16(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        for fmt in (
            make_fmt(audio_format=3, channels=1),
            make_fmt(audio_format=1, channels=1, bits=8, block_align=1, byte_rate=22050),
            make_fmt(audio_format=1, channels=1, bits=24, block_align=3, byte_rate=66150),
        ):
            with self.subTest(fmt=fmt):
                with self.assertRaises(WavContractError):
                    inspect_wav(make_riff([fmt, make_chunk(b"data", pcm)]))

    def test_rejects_zero_or_more_than_two_channels(self) -> None:
        pcm = struct.pack("<hhhh", 1, 2, 3, 4)
        for channels in (0, 3, 8):
            with self.subTest(channels=channels):
                block_align = max(1, channels * 2)
                fmt = make_fmt(
                    channels=channels,
                    block_align=block_align,
                    byte_rate=22050 * block_align,
                )
                with self.assertRaises(WavContractError):
                    inspect_wav(make_riff([fmt, make_chunk(b"data", pcm)]))

    def test_rejects_zero_sample_rate(self) -> None:
        fmt = make_fmt(channels=1, sample_rate=0, byte_rate=0)
        with self.assertRaises(WavContractError):
            inspect_wav(make_riff([fmt, make_chunk(b"data", struct.pack("<h", 1))]))

    def test_rejects_inconsistent_block_align_and_byte_rate(self) -> None:
        pcm = struct.pack("<hhhh", 1, 2, 3, 4)
        bad_align = make_fmt(channels=2, block_align=2, byte_rate=44100)
        bad_rate = make_fmt(channels=2, block_align=4, byte_rate=1)
        for fmt in (bad_align, bad_rate):
            with self.subTest(fmt=fmt):
                with self.assertRaises(WavContractError):
                    inspect_wav(make_riff([fmt, make_chunk(b"data", pcm)]))

    def test_rejects_partial_pcm_frame_and_empty_data(self) -> None:
        stereo = make_fmt(channels=2)
        for payload in (b"", b"\x00\x01", b"\x00\x01\x02"):
            with self.subTest(payload=payload):
                with self.assertRaises(WavContractError):
                    inspect_wav(make_riff([stereo, make_chunk(b"data", payload)]))

    def test_rejects_duplicate_fmt_or_data(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        duplicate_fmt = make_riff([
            make_fmt(channels=1),
            make_fmt(channels=1),
            make_chunk(b"data", pcm),
        ])
        duplicate_data = make_riff([
            make_fmt(channels=1),
            make_chunk(b"data", pcm),
            make_chunk(b"data", pcm),
        ])
        for wav in (duplicate_fmt, duplicate_data):
            with self.subTest(wav=wav):
                with self.assertRaises(WavContractError):
                    inspect_wav(wav)

    def test_admission_uses_decoded_stereo_bytes(self) -> None:
        frames = 4096
        pcm = b"\x00\x00\x00\x00" * frames
        probe = inspect_wav(make_riff([make_fmt(channels=2), make_chunk(b"data", pcm)]))
        self.assertEqual(probe.source_data_bytes, 16384)
        self.assertEqual(probe.decoded_mono_bytes, 8192)
        self.assertTrue(admit_decoded_sample(
            probe,
            pool_free_bytes=8192,
            physical_free_bytes=12000,
            largest_free_block=9000,
        ))
        self.assertFalse(admit_decoded_sample(
            probe,
            pool_free_bytes=8192,
            physical_free_bytes=12000,
            largest_free_block=8191,
        ))

    def test_chunked_stereo_mixdown_matches_expected_for_bounded_scratch(self) -> None:
        frames = [
            (-32768, 32767),
            (-20000, 10000),
            (-1, 0),
            (0, 1),
            (10000, 20000),
            (32767, 32767),
        ]
        pcm = b"".join(struct.pack("<hh", left, right) for left, right in frames)
        wav = make_riff([make_fmt(channels=2), make_chunk(b"data", pcm)])
        probe = inspect_wav(wav)
        expected = b"".join(
            struct.pack("<h", int((int(left) + int(right)) / 2))
            for left, right in frames
        )
        for scratch in (4, 8, 10, 64, 512):
            with self.subTest(scratch=scratch):
                self.assertEqual(decode_stereo_to_mono(wav, probe, scratch), expected)

    def test_stereo_decoder_rejects_scratch_smaller_than_one_frame(self) -> None:
        pcm = struct.pack("<hh", 1, 2)
        wav = make_riff([make_fmt(channels=2), make_chunk(b"data", pcm)])
        probe = inspect_wav(wav)
        for scratch in (0, 1, 2, 3):
            with self.subTest(scratch=scratch):
                with self.assertRaises(ValueError):
                    decode_stereo_to_mono(wav, probe, scratch)


if __name__ == "__main__":
    unittest.main(verbosity=2)
