namespace {
char normalizedSongR4Key(const UIEvent& event) {
  char key = event.key;
  if (event.ctrl) {
    const unsigned char value = static_cast<unsigned char>(key);
    if (value >= 1 && value <= 26) key = static_cast<char>('a' + value - 1);
  }
  return key ? static_cast<char>(std::tolower(static_cast<unsigned char>(key))) : 0;
}

bool songR4GenerationGesture(const UIEvent& event) {
  return event.event_type == GROOVEPUTER_KEY_DOWN &&
         (normalizedSongR4Key(event) == 'g' || event.scancode == GROOVEPUTER_G);
}

bool songR4QueuedReverseGesture(const UIEvent& event) {
  return event.event_type == GROOVEPUTER_KEY_DOWN && event.ctrl &&
         (normalizedSongR4Key(event) == 'r' || event.scancode == GROOVEPUTER_R);
}
}  // namespace

bool SongPage::undoPreparedSongState() {
  using GroovePuterUndo::SongUndoPayload;
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;
  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo() || owner.kind() != UndoKind::Song) return false;

  SongUndoPayload retained{};
  if (!owner.read(UndoKind::Song, retained)) return false;
  if (!GroovePuterUndo::songUndoTargetAvailable(
          mini_acid_.sceneManager(), retained)) {
    return false;  // global direction-aware NOT HERE fallback retains receipt
  }

  const bool redo = owner.nextIsRedo();
  const uint32_t receiptRevision = owner.committedRevision();
  const bool pending =
      GroovePuterRhythm::LiveSongArrangementDetail::
          hasPendingSongActivationForRevision(mini_acid_, receiptRevision);
  const bool affectsAudible =
      GroovePuterRhythm::LiveSongArrangementDetail::
          songUndoWouldAffectAudibleTruth(mini_acid_, retained);

  if (mini_acid_.isPlaying() && affectsAudible && (redo || !pending)) {
    showToast(redo ? "REDO: STOP OR WAIT" : "UNDO: STOP OR WAIT", 1100);
    return true;
  }

  const UndoResult result = owner.togglePrepared<SongUndoPayload>(
      UndoKind::Song,
      [&](const SongUndoPayload& receipt) {
        return GroovePuterUndo::songUndoTargetAvailable(
            mini_acid_.sceneManager(), receipt);
      },
      [&](SongUndoPayload& receipt) {
        if (!redo && pending) {
          GroovePuterRhythm::LiveSongArrangementDetail::
              cancelPendingSongActivationForRevision(
                  mini_acid_, receiptRevision);
        }
        const auto exchange = [&]() {
          GroovePuterUndo::exchangeSongUndo(
              mini_acid_.sceneManager(), receipt);
        };
        if (audio_guard_) audio_guard_(exchange);
        else exchange();
      });
  if (result == UndoResult::Restored) {
    showToast(redo ? "REDO: SONG" : "UNDO: SONG", 900);
    return true;
  }
  if (result == UndoResult::ContextUnavailable ||
      result == UndoResult::TargetUnavailable) {
    return false;
  }
  return result == UndoResult::Expired;
}

bool SongPage::handleEvent(UIEvent& ui_event) {
  using namespace GroovePuterUndo;

  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
    if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {
      return undoPreparedSongState();
    }

    if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_CUT ||
        ui_event.app_event_type == GROOVEPUTER_APP_EVENT_PASTE) {
      const bool wholeSongScope = cursorOnModeButton() || cursorOnPlayheadLabel();
      bool trackValid = false;
      const SongTrack focusedTrack = trackForColumn(cursorTrack(), trackValid);

      if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_CUT) {
        const SceneManager& manager = mini_acid_.sceneManager();
        const int slot = manager.activeSongSlot();
        const Song& source = manager.currentScene().songs[slot];
        if (wholeSongScope) {
          g_song_slot_clipboard.song = source;
          g_song_slot_clipboard.source_slot = slot;
          g_song_slot_clipboard.has_song = true;
          commitSongMutation([&](Song& song) { SongEdit::reset(song); });
          showToast("Song cut", 900);
          return true;
        }
        if (!trackValid) return false;
        if (has_selection_) {
          int minRow, maxRow, minTrack, maxTrack;
          getSelectionBounds(minRow, maxRow, minTrack, maxTrack);
          if (minTrack < 0) minTrack = 0;
          const int maxCol = maxPatternTrackColumn();
          if (maxTrack > maxCol) maxTrack = maxCol;
          if (minTrack > maxTrack) return false;
          g_song_area_clipboard.rows = maxRow - minRow + 1;
          g_song_area_clipboard.tracks = maxTrack - minTrack + 1;
          g_song_area_clipboard.pattern_indices.clear();
          g_song_area_clipboard.pattern_indices.reserve(
              g_song_area_clipboard.rows * g_song_area_clipboard.tracks);
          for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minTrack; col <= maxTrack; ++col) {
              bool valid = false;
              const SongTrack track = trackForColumn(col, valid);
              g_song_area_clipboard.pattern_indices.push_back(
                  valid ? SongEdit::patternAt(source, row, track) : -1);
            }
          }
          g_song_area_clipboard.has_area = true;
          g_song_pattern_clipboard.has_pattern = false;
          commitSongMutation([&](Song& song) {
            for (int row = minRow; row <= maxRow; ++row) {
              for (int col = minTrack; col <= maxTrack; ++col) {
                bool valid = false;
                const SongTrack track = trackForColumn(col, valid);
                if (valid) SongEdit::clearPattern(song, row, track);
              }
            }
          });
          return true;
        }
        g_song_pattern_clipboard.pattern_index =
            SongEdit::patternAt(source, cursorRow(), focusedTrack);
        g_song_pattern_clipboard.has_pattern = true;
        g_song_area_clipboard.has_area = false;
        commitSongMutation([&](Song& song) {
          SongEdit::clearPattern(song, cursorRow(), focusedTrack);
        });
        return true;
      }

      if (wholeSongScope) {
        if (!g_song_slot_clipboard.has_song) return false;
        const Song pasted = g_song_slot_clipboard.song;
        commitSongMutation([&](Song& song) { song = pasted; });
        showToast("Song pasted", 900);
        return true;
      }
      if (!trackValid) return false;
      if (g_song_area_clipboard.has_area) {
        int startRow = cursorRow();
        int startTrack = cursorTrack();
        bool selectionAnchor = false;
        if (has_selection_) {
          int minRow, maxRow, minCol, maxCol;
          getSelectionBounds(minRow, maxRow, minCol, maxCol);
          (void)maxRow;
          (void)maxCol;
          startRow = minRow;
          startTrack = minCol;
          selectionAnchor = true;
        }
        const int maxCol = maxPatternTrackColumn();
        if (startTrack > maxCol) return false;
        const int sourceRows = g_song_area_clipboard.rows;
        const int sourceTracks = g_song_area_clipboard.tracks;
        if (sourceRows <= 0 || sourceTracks <= 0) return false;
        int pasteTracks = std::min(sourceTracks, maxCol + 1);
        if (!selectionAnchor && startTrack + pasteTracks - 1 > maxCol) {
          startTrack = maxCol - pasteTracks + 1;
        }
        if (startTrack < 0) startTrack = 0;
        pasteTracks = std::min(pasteTracks, maxCol - startTrack + 1);
        if (pasteTracks <= 0) return false;
        commitSongMutation([&](Song& song) {
          for (int rowOffset = 0; rowOffset < sourceRows; ++rowOffset) {
            for (int trackOffset = 0; trackOffset < pasteTracks; ++trackOffset) {
              const int row = startRow + rowOffset;
              const int col = startTrack + trackOffset;
              const int sourceIndex = rowOffset * sourceTracks + trackOffset;
              if (row < 0 || row >= Song::kMaxPositions ||
                  sourceIndex < 0 || sourceIndex >= static_cast<int>(
                      g_song_area_clipboard.pattern_indices.size())) continue;
              bool valid = false;
              const SongTrack track = trackForColumn(col, valid);
              if (!valid) continue;
              SongEdit::setPattern(
                  song, row, track,
                  g_song_area_clipboard.pattern_indices[sourceIndex]);
            }
          }
        });
        if (has_selection_) clearSelection();
        return true;
      }
      if (!g_song_pattern_clipboard.has_pattern) return false;
      const int pattern = g_song_pattern_clipboard.pattern_index;
      commitSongMutation([&](Song& song) {
        SongEdit::setPattern(song, cursorRow(), focusedTrack, pattern);
      });
      if (has_selection_) clearSelection();
      return true;
    }

    return handleEventLegacyUnowned(ui_event);
  }

  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return handleEventLegacyUnowned(ui_event);
  }

  // Generation owns its existing PREPARE/commit path. Queued reverse belongs
  // to 0.9.9 ACTIVATE; R4 only expires a stale arrangement receipt when a new
  // reverse transition is actually scheduled.
  if (songR4GenerationGesture(ui_event)) {
    return handleEventLegacyUnowned(ui_event);
  }
  if (songR4QueuedReverseGesture(ui_event)) {
    // Legacy gesture timing/long-press UX is preserved, but the actual reverse
    // mutation now routes through commitSongMutation inside the legacy handler.
    return handleEventLegacyUnowned(ui_event);
  }

  const int nav = UIInput::navCode(ui_event);
  if (ui_event.alt && (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) &&
      !cursorOnPlayheadLabel()) {
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    const int row = cursorRow();
    const Song& currentSong = mini_acid_.sceneManager().currentScene().songs[
        mini_acid_.activeSongSlot()];
    const int current = SongEdit::patternAt(currentSong, row, track);
    int next = current;
    if (nav == GROOVEPUTER_UP) next = current < 0 ? 0 : current + 1;
    else next = current < 0 ? -1 : current - 1;
    next = std::max(-1, std::min(kMaxGlobalPatterns - 1, next));
    if (next == current) return false;
    const bool committed = commitSongMutation([&](Song& song) {
      SongEdit::setPattern(song, row, track, next);
    });
    if (committed && mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      withAudioGuard([&]() { mini_acid_.setSongPosition(row); });
    }
    return committed;
  }

  const char lowerKey = normalizedSongR4Key(ui_event);
  const bool keyB = lowerKey == 'b' || ui_event.scancode == GROOVEPUTER_B;
  const bool keyM = lowerKey == 'm' || ui_event.scancode == GROOVEPUTER_M;
  const bool keyN = lowerKey == 'n' || ui_event.scancode == GROOVEPUTER_N;

  if (ui_event.alt && !ui_event.ctrl && keyB) {
    int changed = 0;
    if (has_selection_) {
      int minRow, maxRow, minTrack, maxTrack;
      getSelectionBounds(minRow, maxRow, minTrack, maxTrack);
      if (minTrack < 0) minTrack = 0;
      const int maxCol = maxPatternTrackColumn();
      if (maxTrack > maxCol) maxTrack = maxCol;
      if (minTrack > maxTrack) return false;
      const bool committed = commitSongMutation([&](Song& song) {
        for (int row = minRow; row <= maxRow; ++row) {
          for (int col = minTrack; col <= maxTrack; ++col) {
            bool valid = false;
            const SongTrack track = trackForColumn(col, valid);
            if (!valid) continue;
            const int current = SongEdit::patternAt(song, row, track);
            const int next = SongEdit::flipBankReference(current);
            if (next != current) {
              SongEdit::setPattern(song, row, track, next);
              ++changed;
            }
          }
        }
      });
      clearSelection();
      if (committed) {
        char toast[40];
        std::snprintf(toast, sizeof(toast), "Bank flip: %d", changed);
        showToast(toast, 900);
      }
      return committed;
    }
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    const int row = cursorRow();
    const bool committed = commitSongMutation([&](Song& song) {
      const int current = SongEdit::patternAt(song, row, track);
      SongEdit::setPattern(song, row, track, SongEdit::flipBankReference(current));
    });
    if (committed) showToast("Bank flip", 700);
    return committed;
  }

  if (ui_event.ctrl && !ui_event.alt && keyM) {
    const int row = cursorRow();
    commitSongMutation([&](Song& song) { SongEdit::deleteRow(song, row); });
    cursor_row_ = clampCursorRow(cursor_row_);
    char toast[32];
    std::snprintf(toast, sizeof(toast), "DEL row %d", row + 1);
    showToast(toast, 900);
    return true;
  }
  if (ui_event.ctrl && !ui_event.alt && keyN) {
    const int row = cursorRow();
    commitSongMutation([&](Song& song) { SongEdit::insertRow(song, row); });
    char toast[32];
    std::snprintf(toast, sizeof(toast), "INS row %d", row + 1);
    showToast(toast, 900);
    return true;
  }

  int patternIdx = patternIndexFromKey(lowerKey);
  if (patternIdx < 0) patternIdx = scancodeToPatternIndex(ui_event.scancode);
  if (patternIdx >= 0 && !ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (cursorOnModeButton()) return false;
    const int combined = songPatternFromPageBankIndex(
        mini_acid_.currentPageIndex(), assignment_bank_index_, patternIdx);
    if (has_selection_) {
      int minRow, maxRow, minTrack, maxTrack;
      getSelectionBounds(minRow, maxRow, minTrack, maxTrack);
      if (minTrack < 0) minTrack = 0;
      const int maxCol = maxPatternTrackColumn();
      if (maxTrack > maxCol) maxTrack = maxCol;
      if (minTrack > maxTrack) return false;
      int changed = 0;
      int writes = 0;
      commitSongMutation([&](Song& song) {
        for (int row = minRow; row <= maxRow; ++row) {
          for (int col = minTrack; col <= maxTrack; ++col) {
            bool valid = false;
            const SongTrack track = trackForColumn(col, valid);
            if (!valid) continue;
            const int old = SongEdit::patternAt(song, row, track);
            SongEdit::setPattern(song, row, track, combined);
            ++writes;
            if (old != SongEdit::patternAt(song, row, track)) ++changed;
          }
        }
      });
      clearSelection();
      char toast[48];
      std::snprintf(toast, sizeof(toast), "Fill %d/%d -> P%d",
                    changed, writes, patternIdx + 1);
      showToast(toast, 1200);
      return true;
    }
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    const int row = cursorRow();
    commitSongMutation([&](Song& song) {
      SongEdit::setPattern(song, row, track, combined);
    });
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      withAudioGuard([&]() { mini_acid_.setSongPosition(row); });
    }
    return true;
  }

  const bool backspace = ui_event.key == '\b' || ui_event.key == 0x7F;
  if (ui_event.alt && backspace) {
    commitSongMutation([&](Song& song) { SongEdit::reset(song); });
    showToast("Song Cleared", 900);
    return true;
  }
  if (!ui_event.alt && (backspace || ui_event.key == '\t')) {
    if (has_selection_) {
      int minRow, maxRow, minTrack, maxTrack;
      getSelectionBounds(minRow, maxRow, minTrack, maxTrack);
      if (minTrack < 0) minTrack = 0;
      const int maxCol = maxPatternTrackColumn();
      if (maxTrack > maxCol) maxTrack = maxCol;
      if (minTrack > maxTrack) return false;
      int cleared = 0;
      commitSongMutation([&](Song& song) {
        for (int row = minRow; row <= maxRow; ++row) {
          for (int col = minTrack; col <= maxTrack; ++col) {
            bool valid = false;
            const SongTrack track = trackForColumn(col, valid);
            if (!valid) continue;
            if (SongEdit::patternAt(song, row, track) >= 0) ++cleared;
            SongEdit::clearPattern(song, row, track);
          }
        }
      });
      clearSelection();
      char toast[48];
      std::snprintf(toast, sizeof(toast), "Cleared %d cells", cleared);
      showToast(toast, 900);
      return true;
    }
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    commitSongMutation([&](Song& song) {
      SongEdit::clearPattern(song, cursorRow(), track);
    });
    return true;
  }

  return handleEventLegacyUnowned(ui_event);
}
