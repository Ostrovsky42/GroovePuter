#!/usr/bin/env bash
set -u
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0

require_file() {
  local path="$1"
  if [[ ! -f "${ROOT_DIR}/${path}" ]]; then
    echo "C9 USER SURFACE RED: missing ${path}"
    status=1
  fi
}

require_text() {
  local path="$1"
  local pattern="$2"
  local label="$3"
  if [[ ! -f "${ROOT_DIR}/${path}" ]] || ! grep -Fq -- "$pattern" "${ROOT_DIR}/${path}"; then
    echo "C9 USER SURFACE RED: ${label}"
    status=1
  fi
}

require_file "src/midi/midi_input_settings.h"
require_file "src/ui/midi_input_ui.h"
require_text "src/platform/cardputer_midi_settings_session.h" \
  "initializeCardputerMidiInputSettings" \
  "missing persisted MIDI input initialization seam"
require_text "src/platform/cardputer_midi_settings_session.h" \
  "setCardputerMidiInputRoutingConfig" \
  "missing durable MIDI input mutation seam"
require_text "src/ui/pages/project_page.h" \
  "MidiInputEnabled" \
  "Project/MIDI does not expose input enable focus"
require_text "src/ui/pages/project_page.h" \
  "MidiInputChannel" \
  "Project/MIDI does not expose input channel focus"
require_text "src/ui/pages/project_page.h" \
  "MidiInputTarget" \
  "Project/MIDI does not expose input target focus"
require_text "GroovePuter.ino" \
  "initializeCardputerMidiInputSettings" \
  "boot does not restore persisted MIDI input config"

if [[ ${status} -ne 0 ]]; then
  exit ${status}
fi

echo "C9 MIDI user surface contract: PASS"
