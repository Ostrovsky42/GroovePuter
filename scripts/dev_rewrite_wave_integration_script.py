#!/usr/bin/env python3
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit("usage: dev_rewrite_wave_integration_script.py <script>")

target = Path(sys.argv[1])
text = target.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '''text = replace_once(
    text,
    '    eventQueue_.invalidateAndRequestPanic();\\n    projectLaunchPlanned_ = false;\\n',
    '    eventQueue_.invalidateAndRequestPanic();\\n    resetMidiVisual();\\n    projectLaunchPlanned_ = false;\\n',
    "original start visual reset")
''',
    '''text = replace_once(
    text,
    'bool CardputerSmfPlayerService::startOriginalFromTick(uint32_t tick) {\\n'
    '    if (!loaded_ || !timing_.valid()) return false;\\n'
    '    if (tick > endTick_) tick = musicStartTick_;\\n\\n'
    '    eventQueue_.invalidateAndRequestPanic();\\n'
    '    projectLaunchPlanned_ = false;\\n',
    'bool CardputerSmfPlayerService::startOriginalFromTick(uint32_t tick) {\\n'
    '    if (!loaded_ || !timing_.valid()) return false;\\n'
    '    if (tick > endTick_) tick = musicStartTick_;\\n\\n'
    '    eventQueue_.invalidateAndRequestPanic();\\n'
    '    resetMidiVisual();\\n'
    '    projectLaunchPlanned_ = false;\\n',
    "original start visual reset")
''',
    "narrow original start replacement",
)

replace_once(
    '''text = replace_once(
    text,
    '    snapshot_.launchMode = launchMode_;\\n    portEXIT_CRITICAL(&snapshotMux_);\\n',
    '    snapshot_.launchMode = launchMode_;\\n'
    '    snapshot_.midiVisual = midiVisual;\\n'
    '    portEXIT_CRITICAL(&snapshotMux_);\\n',
    "publish visual snapshot")
''',
    '''text = replace_once(
    text,
    '    const SmfMidiVisualSnapshot midiVisual = midiVisualTimeline_.advanceTo(tick);\\n\\n'
    '    portENTER_CRITICAL(&snapshotMux_);\\n'
    '    snapshot_.currentTick = tick;\\n'
    '    snapshot_.bar = pos.bar;\\n'
    '    snapshot_.beat = pos.beat;\\n'
    '    snapshot_.originalBpmX10 = originalBpmX10;\\n'
    '    snapshot_.bpmX10 = bpmX10;\\n'
    '    snapshot_.tempoScalePermille = tempoScalePermille_;\\n'
    '    snapshot_.velocityBoost = velocityBoost_;\\n'
    '    snapshot_.tempoMode = tempoMode_;\\n'
    '    snapshot_.launchMode = launchMode_;\\n'
    '    portEXIT_CRITICAL(&snapshotMux_);\\n',
    '    const SmfMidiVisualSnapshot midiVisual = midiVisualTimeline_.advanceTo(tick);\\n\\n'
    '    portENTER_CRITICAL(&snapshotMux_);\\n'
    '    snapshot_.currentTick = tick;\\n'
    '    snapshot_.bar = pos.bar;\\n'
    '    snapshot_.beat = pos.beat;\\n'
    '    snapshot_.originalBpmX10 = originalBpmX10;\\n'
    '    snapshot_.bpmX10 = bpmX10;\\n'
    '    snapshot_.tempoScalePermille = tempoScalePermille_;\\n'
    '    snapshot_.velocityBoost = velocityBoost_;\\n'
    '    snapshot_.tempoMode = tempoMode_;\\n'
    '    snapshot_.launchMode = launchMode_;\\n'
    '    snapshot_.midiVisual = midiVisual;\\n'
    '    portEXIT_CRITICAL(&snapshotMux_);\\n',
    "publish visual snapshot")
''',
    "anchor playback snapshot replacement",
)

replace_once(
    '''if 'test_smf_midi_visual' not in text:
    marker = '\"${BUILD_DIR}/test_smf_stream\"\\n'
    text = replace_once(text, marker, marker + blocks, "visual test runner")
''',
    '''if 'test_smf_midi_visual' not in text:
    marker = '\\n\"${BUILD_DIR}/test_smf_stream\"\\n\\n'
    text = replace_once(text, marker, marker + blocks + "\\n", "visual test runner")
''',
    "anchor visual test runner",
)

target.write_text(text, encoding="utf-8")
print("wave integration lifecycle replacements narrowed")
