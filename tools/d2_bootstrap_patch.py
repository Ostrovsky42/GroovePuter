#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one exact anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


engine = ROOT / "src/dsp/miniacid_engine.cpp"
replace_once(
    engine,
    """  if (barTick == 0) {
    advanceSongBar_();
    // Also regenerate if needed at bar start
    if (genreManager_.commitPendingRecipe()) {
      regeneratePatternsWithGenre();
    }
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
""",
    """  if (barTick == 0) {
    // 0.9.9-D2: ACTIVATE the already-committed pending mutation before Song
    // row advancement. Phrase live arrangement changes persistent Song refs and
    // feel.patternBars during COMMIT, while the current bar keeps the old C
    // audible overlay. Activating first makes the normal advanceSongBar_() turn
    // the new destination's pre-first-bar phase into bar zero without exposing
    // a transient next-row Voice/Pattern selection or stretching bar one.
    // The hook remains the single 0.9.9-C pending owner and returns false; no
    // generation, Scene write, allocation, filesystem work or second Undo is
    // allowed on this audio-thread boundary.
    if (genreManager_.commitPendingRecipe()) {
      regeneratePatternsWithGenre();
    }
    advanceSongBar_();
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
""",
)

replace_once(
    engine,
    """void MiniAcid::stop() {
  if (GroovePuterRhythm::QuantizedGenerationDetail::
          cancelPendingGenerationActivation(*this)) {
    GroovePuterRhythm::QuantizedGenerationDetail::
        synchronizeCommittedGenerationRuntime(*this);
  }
""",
    """void MiniAcid::stop() {
  // Phrase D2 has already committed persistent Song/Pattern truth. STOP must
  // settle that exact pending destination immediately instead of discarding the
  // activation and leaving the next START on the old runtime row. Ordinary C
  // generation keeps its existing cancel+runtime-settlement behavior.
  const bool phraseSettled =
      GroovePuterRhythm::PhraseLiveArrangementDetail::
          settlePendingPhraseArrangementOnStop(*this);
  if (!phraseSettled &&
      GroovePuterRhythm::QuantizedGenerationDetail::
          cancelPendingGenerationActivation(*this)) {
    GroovePuterRhythm::QuantizedGenerationDetail::
        synchronizeCommittedGenerationRuntime(*this);
  }
""",
)

phrase = ROOT / "src/ui/pages/phrase_page.cpp"
replace_once(
    phrase,
    """bool PhrasePage::generatePhraseToSong() {
  if (mini_acid_.isPlaying()) {
    LOG_WARN_UI(\"Generated Phrase -> Song rejected while transport is playing\");
    UI::showToast(\"STOP PLAYBACK FOR PHRASE\", 1400);
    return true;
  }

  const int songStart = static_cast<int>(destination_row_);
  const PhraseGenerator::PhraseResult result = GeneratedPhraseSong::generate(
      mini_acid_, capture_length_, songStart,
      [&](auto&& operation) {
        if (audio_guard_) {
          audio_guard_(std::forward<decltype(operation)>(operation));
        } else {
          operation();
        }
      });

  if (!result) {
    LOG_WARN_UI(\"Generated Phrase -> Song failed at TO=%d: %s\",
                songStart + 1,
                PhraseGenerator::errorText(result.error));
    UI::showToast(PhraseGenerator::errorText(result.error), 1600);
    return true;
  }

  preview_bar_ = 0;
  const int nextRow = std::min(
      Song::kMaxPositions - 1,
      static_cast<int>(songStart) + result.bars);
  destination_row_ = static_cast<uint8_t>(nextRow);
  invalidatePreview();

  char message[64];
  std::snprintf(message, sizeof(message), \"%dB GEN -> SONG %d-%d\",
                result.bars,
                result.songStart + 1,
                result.songStart + result.bars);
  UI::showToast(message, 1600);
  LOG_INFO_UI(\"Generated %dB phrase -> Song rows %d..%d page=%d firstPattern=%d\",
              result.bars,
              result.songStart + 1,
              result.songStart + result.bars,
              mini_acid_.currentPageIndex() + 1,
              result.firstGlobalPattern);
  return true;
}
""",
    """bool PhrasePage::generatePhraseToSong() {
  const int songStart = static_cast<int>(destination_row_);
  const GeneratedPhraseSong::Result result = GeneratedPhraseSong::generate(
      mini_acid_, capture_length_, songStart,
      [&](auto&& operation) {
        if (audio_guard_) {
          audio_guard_(std::forward<decltype(operation)>(operation));
        } else {
          operation();
        }
      });

  if (!result) {
    LOG_WARN_UI(\"Generated Phrase -> Song failed at TO=%d: %s\",
                songStart + 1, GeneratedPhraseSong::statusText(result));
    UI::showToast(GeneratedPhraseSong::statusText(result), 1600);
    return true;
  }

  const PhraseGenerator::PhraseResult& phraseResult = result.phrase;
  preview_bar_ = 0;
  const int nextRow = std::min(
      Song::kMaxPositions - 1,
      static_cast<int>(songStart) + phraseResult.bars);
  destination_row_ = static_cast<uint8_t>(nextRow);
  invalidatePreview();

  char message[64];
  if (result.status == GeneratedPhraseSong::LifecycleStatus::PendingNextBar) {
    std::snprintf(message, sizeof(message), \"%dB GEN -> NEXT BAR %d-%d\",
                  phraseResult.bars,
                  phraseResult.songStart + 1,
                  phraseResult.songStart + phraseResult.bars);
  } else {
    std::snprintf(message, sizeof(message), \"%dB GEN -> SONG %d-%d\",
                  phraseResult.bars,
                  phraseResult.songStart + 1,
                  phraseResult.songStart + phraseResult.bars);
  }
  UI::showToast(message, 1600);
  LOG_INFO_UI(\"Generated %dB phrase -> Song rows %d..%d page=%d firstPattern=%d status=%s\",
              phraseResult.bars,
              phraseResult.songStart + 1,
              phraseResult.songStart + phraseResult.bars,
              mini_acid_.currentPageIndex() + 1,
              phraseResult.firstGlobalPattern,
              GeneratedPhraseSong::statusText(result));
  return true;
}
""",
)

replace_once(
    phrase,
    """  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo()) return false;

  if (owner.kind() == UndoKind::Phrase) {
""",
    """  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo()) return false;

  if (owner.kind() == UndoKind::Generation &&
      GeneratedPhraseSong::ownsCurrentUndoReceipt()) {
    const UndoResult result = GeneratedPhraseSong::undoLastGeneratedPhrase(
        mini_acid_,
        [&](auto&& operation) {
          if (audio_guard_) {
            audio_guard_(std::forward<decltype(operation)>(operation));
          } else {
            operation();
          }
        });
    if (result == UndoResult::Restored) {
      invalidatePreview();
      UI::showToast(\"UNDO: GENERATED PHRASE\", 1000);
      return true;
    }
    if (result == UndoResult::TargetUnavailable) {
      UI::showToast(mini_acid_.isPlaying()
                        ? \"UNDO: STOP OR WAIT\"
                        : \"UNDO: RETURN PAGE\",
                    1200);
      return true;
    }
    return result == UndoResult::Expired;
  }

  if (owner.kind() == UndoKind::Phrase) {
""",
)

print("D2 exact integration anchors patched")
