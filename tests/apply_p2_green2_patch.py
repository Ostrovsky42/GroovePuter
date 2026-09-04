#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/dsp/miniacid_engine.cpp")
HDR = Path("src/dsp/miniacid_engine.h")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one old fragment, found {count}")
    return text.replace(old, new, 1)


def rewrite_function(text: str, start: str, end: str, edits) -> str:
    begin = text.index(start)
    finish = text.index(end, begin + len(start))
    body = text[begin:finish]
    for old, new, label in edits:
        body = replace_once(body, old, new, label)
    return text[:begin] + body + text[finish:]


cpp = CPP.read_text(encoding="utf-8")
hdr = HDR.read_text(encoding="utf-8")

header_old = """  void consumePatternPlaybackActions_(
      int synthIdx,
      const PhraseRuntime::RuntimeSynthPlaybackActions& actions);
"""
header_new = header_old + """  void hardBarrierPatternPlayback_();
  void cleanupLiveNotesForTransportBarrier_(uint8_t patternAuthorityAtEntry);
"""
hdr = replace_once(hdr, header_old, header_new, "header helper declarations")

stop_snapshot = "void MiniAcid::stop() {\n"
stop_snapshot_new = """void MiniAcid::stop() {
  const uint8_t patternAuthorityAtEntry =
      patternOwnedMask_.load(std::memory_order_acquire);
"""
release_block = """  if (synthVoices_[0]) synthVoices_[0]->release();
  if (synthVoices_[1]) synthVoices_[1]->release();
  liveNotes_[0] = -1;
  liveNotes_[1] = -1;
"""
cleanup_call = "  cleanupLiveNotesForTransportBarrier_(patternAuthorityAtEntry);\n"
cpp = rewrite_function(
    cpp,
    "void MiniAcid::stop()",
    "void MiniAcid::pauseTransport()",
    [
        (stop_snapshot, stop_snapshot_new, "STOP authority snapshot"),
        ("  publishPatternAllNotesOff_();\n", "  hardBarrierPatternPlayback_();\n", "STOP Pattern barrier"),
        (release_block, cleanup_call, "STOP live cleanup"),
    ],
)

pause_snapshot = """void MiniAcid::pauseTransport() {
  if (!playing) return;
"""
pause_snapshot_new = """void MiniAcid::pauseTransport() {
  if (!playing) return;
  const uint8_t patternAuthorityAtEntry =
      patternOwnedMask_.load(std::memory_order_acquire);
"""
cpp = rewrite_function(
    cpp,
    "void MiniAcid::pauseTransport()",
    "void MiniAcid::continueTransport()",
    [
        (pause_snapshot, pause_snapshot_new, "PAUSE authority snapshot"),
        ("  publishPatternAllNotesOff_();\n", "  hardBarrierPatternPlayback_();\n", "PAUSE Pattern barrier"),
        (release_block, cleanup_call, "PAUSE live cleanup"),
    ],
)

consumer_marker = "void MiniAcid::consumePatternPlaybackActions_(\n"
if cpp.count(consumer_marker) != 1:
    raise SystemExit("consumer marker is not unique")
helpers = """void MiniAcid::hardBarrierPatternPlayback_() {
  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
    const auto actions = patternPlaybackState_[synth].hardBarrier();
    consumePatternPlaybackActions_(synth, actions);
  }
}

void MiniAcid::cleanupLiveNotesForTransportBarrier_(
    uint8_t patternAuthorityAtEntry) {
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    const uint8_t targetMask = static_cast<uint8_t>(1u << idx);
    const bool patternOwnedBackendAtEntry =
        (patternAuthorityAtEntry & targetMask) != 0u;
    if (!patternOwnedBackendAtEntry && liveNotes_[idx] >= 0 &&
        synthVoices_[idx]) {
      synthVoices_[idx]->release();
    }
    liveNotes_[idx] = -1;
  }
}

"""
cpp = cpp.replace(consumer_marker, helpers + consumer_marker, 1)

CPP.write_text(cpp, encoding="utf-8")
HDR.write_text(hdr, encoding="utf-8")
