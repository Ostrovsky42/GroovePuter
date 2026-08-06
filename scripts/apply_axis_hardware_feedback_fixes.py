#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file_path = Path(path)
    text = file_path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"{label} anchor not found in {path}")
    file_path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/ui/global_help_content.h",
    '''constexpr const char* kGenerationLines[] = {
    "=== GENERATION 3/4 ===",
    "Generation = material/development",
    "Enter/G     Materialize current bar",
    "SCOPE       Current Song row",
    "PLAN        Single bar / base",
    "A/S/FILL    Generation probabilities",
    "Phrase len  Owned by PHRASE CORE",
    "Linear constructive pass",
    "No scoring or retry loop",
    "No texture or microtiming changes",
};
''',
    '''constexpr const char* kGenerationLines[] = {
    "=== GENERATION 3/4 ===",
    "Generation = material/development",
    "Arrows      Select target Song row",
    "Hold arrows Accelerate target move",
    "Enter/G     Materialize selected row",
    "PLAN        Single bar / base",
    "A/S/FILL    Generation probabilities",
    "Phrase len  Owned by PHRASE CORE",
    "Linear constructive pass",
    "No scoring or retry loop",
    "No texture or microtiming changes",
};
''',
    "Generation help block",
)

replace_once(
    "GroovePuter.ino",
    '''      if (GroovePuterInput::mayRepeat(evt) &&
          ks.hid_keys.size() == 1 && ks.word.empty()) {
        repeatEvent = evt;
''',
    '''      const bool navigationRepeat =
          evt.scancode == GROOVEPUTER_UP ||
          evt.scancode == GROOVEPUTER_DOWN ||
          evt.scancode == GROOVEPUTER_LEFT ||
          evt.scancode == GROOVEPUTER_RIGHT;
      if (GroovePuterInput::mayRepeat(evt) &&
          ks.hid_keys.size() == 1 &&
          (ks.word.empty() || navigationRepeat)) {
        repeatEvent = evt;
''',
    "Cardputer navigation repeat",
)

replace_once(
    "src/ui/pages/generation_page.cpp",
    '''void GenerationPage::moveTargetRow(int delta, bool fast) {
  if (delta == 0) return;
  const int multiplier = hold_accel_.multiplier(delta, fast);
  target_row_ = clampSongRow(target_row_ + delta * multiplier);
  last_attempted_ = false;
}
''',
    '''void GenerationPage::moveTargetRow(int delta, bool fast) {
  if (delta == 0) return;
  const int previousRow = target_row_;
  const int multiplier = hold_accel_.multiplier(delta, fast);
  target_row_ = clampSongRow(target_row_ + delta * multiplier);
  last_attempted_ = false;

  if (target_row_ == previousRow) return;

  Serial.printf("[GENERATION] target %d -> %d delta=%d mult=%d\\n",
                previousRow + 1, target_row_ + 1, delta, multiplier);
  char toast[48];
  std::snprintf(toast, sizeof(toast), "GEN TARGET ROW %d", target_row_ + 1);
  UI::showToast(toast, 650);
}
''',
    "Generation target feedback",
)

replace_once(
    "src/ui/pages/generation_page.cpp",
    '''  const int targetRow = clampSongRow(target_row_);
  const auto generate = [&]() {
''',
    '''  const int targetRow = clampSongRow(target_row_);
  Serial.printf("[GENERATION] write request row=%d\\n", targetRow + 1);
  const auto generate = [&]() {
''',
    "Generation write request feedback",
)

replace_once(
    "src/ui/pages/generation_page.cpp",
    '''    UI::showToast(toast, 2000);
  } else {
''',
    '''    UI::showToast(toast, 2000);
    Serial.printf("[GENERATION] write ok row=%d pattern=%d\\n",
                  result.songStart + 1, result.firstGlobalPattern);
  } else {
''',
    "Generation write success feedback",
)

replace_once(
    "src/ui/pages/generation_page.cpp",
    '''    UI::showToast(toast, 2200);
  }

  if (wasPlaying) mini_acid_.start();
''',
    '''    UI::showToast(toast, 2200);
    Serial.printf("[GENERATION] write blocked row=%d error=%s\\n",
                  targetRow + 1, last_status_.c_str());
  }

  if (wasPlaying) mini_acid_.start();
''',
    "Generation write failure feedback",
)
