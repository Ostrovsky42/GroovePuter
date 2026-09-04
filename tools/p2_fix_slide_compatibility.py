#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/dsp/miniacid_engine.cpp"
text = PATH.read_text(encoding="utf-8")

old = '''    const PhraseRuntime::RuntimeSynthPlaybackAction& action = actions.values[i];
    const PhraseRuntime::RuntimeSynthEvent& event = action.event;
    const bool accent = (event.flags & PhraseRuntime::kEventAccent) != 0;
    const bool slide = (event.flags & PhraseRuntime::kEventSlide) != 0;

    switch (action.type) {
      case PhraseRuntime::RuntimeSynthPlaybackActionType::Release:
        if (synthVoices_[idx]) synthVoices_[idx]->release();
        publishPatternNoteOff_(idx);
        break;
'''
new = '''    const PhraseRuntime::RuntimeSynthPlaybackAction& action = actions.values[i];
    const PhraseRuntime::RuntimeSynthEvent& event = action.event;
    const bool accent = (event.flags & PhraseRuntime::kEventAccent) != 0;
    const bool slide = (event.flags & PhraseRuntime::kEventSlide) != 0;

    // RuntimeSynthPlaybackState owns the logical replacement as Release ->
    // Start. TB303's accepted legacy slide, however, is legato only while the
    // internal gate remains high. Translate a slide replacement without an
    // internal gate-off while still closing/reopening external MIDI ownership.
    const bool slideReplacement =
        action.type == PhraseRuntime::RuntimeSynthPlaybackActionType::Release &&
        i + 1u < actions.count &&
        actions.values[i + 1u].type ==
            PhraseRuntime::RuntimeSynthPlaybackActionType::Start &&
        (actions.values[i + 1u].event.flags & PhraseRuntime::kEventSlide) != 0;
    const bool skipInternalRelease = slideReplacement;

    switch (action.type) {
      case PhraseRuntime::RuntimeSynthPlaybackActionType::Release:
        if (!skipInternalRelease && synthVoices_[idx]) {
          synthVoices_[idx]->release();
        }
        publishPatternNoteOff_(idx);
        break;
'''
if text.count(old) != 1:
    raise RuntimeError(f"expected one common consumer release block, got {text.count(old)}")
PATH.write_text(text.replace(old, new, 1), encoding="utf-8")
print("P2 TB303 legato-slide backend translation applied")
