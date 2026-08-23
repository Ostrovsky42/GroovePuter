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

bool songPatternPickerOpenGesture(const UIEvent& event) {
  return event.event_type == GROOVEPUTER_KEY_DOWN && event.alt && !event.ctrl &&
         (event.key == '\n' || event.key == '\r');
}

int pickerLocalSlot(int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  if (!address.valid()) return -1;
  return address.bank * Bank<SynthPattern>::kPatterns + address.slot;
}

const char* pickerTrackName(SongTrack track) {
  switch (track) {
    case SongTrack::SynthA: return "A";
    case SongTrack::SynthB: return "B";
    case SongTrack::Drums: return "DR";
    case SongTrack::Voice: return "VO";
  }
  return "?";
}
}  // namespace

bool SongPage::pickerTrackHasMaterial(int globalPattern) const {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  if (!address.valid() || address.page != mini_acid_.currentPageIndex()) {
    return false;
  }
  const int localSlot =
      address.bank * Bank<SynthPattern>::kPatterns + address.slot;
  return !SongPatternMaterializer::slotContentIsEmpty(
      mini_acid_.sceneManager().currentScene(), pattern_picker_.track, localSlot);
}

int SongPage::findExistingPickerPattern(int startLocalSlot,
                                        int direction,
                                        int bankConstraint) const {
  if (direction == 0) direction = 1;
  int start = startLocalSlot;
  if (start < 0 || start >= kPatternsPerPage) start = 0;
  const int page = mini_acid_.currentPageIndex();
  for (int offset = 0; offset < kPatternsPerPage; ++offset) {
    int local = start + direction * offset;
    while (local < 0) local += kPatternsPerPage;
    while (local >= kPatternsPerPage) local -= kPatternsPerPage;
    const int bank = local / Bank<SynthPattern>::kPatterns;
    if (bankConstraint >= 0 && bank != bankConstraint) continue;
    const int slot = local % Bank<SynthPattern>::kPatterns;
    const int global = songPatternFromPageBankIndex(page, bank, slot);
    if (pickerTrackHasMaterial(global)) return global;
  }
  return -1;
}

bool SongPage::applyPatternPickerPreview(int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  if (!address.valid() || address.page != mini_acid_.currentPageIndex()) {
    return false;
  }

  withRuntimeAudioGuard([&]() {
    switch (pattern_picker_.track) {
      case SongTrack::SynthA:
        mini_acid_.set303BankIndex(0, address.bank);
        mini_acid_.set303PatternIndex(0, address.slot);
        break;
      case SongTrack::SynthB:
        mini_acid_.set303BankIndex(1, address.bank);
        mini_acid_.set303PatternIndex(1, address.slot);
        break;
      case SongTrack::Drums:
        mini_acid_.setDrumBankIndex(address.bank);
        mini_acid_.setDrumPatternIndex(address.slot);
        break;
      case SongTrack::Voice:
        break;
    }
  });
  return pattern_picker_.track != SongTrack::Voice;
}

void SongPage::enterPatternPickerAuditionRuntime() {
  // Song arrangement truth remains untouched during audition. Song mode is a
  // runtime selector, so temporarily fall back to the already-existing pattern
  // player and restore it exactly when the modal closes.
  if (mini_acid_.songModeEnabled()) {
    withRuntimeAudioGuard([&]() { mini_acid_.setSongMode(false); });
  }
}

void SongPage::restorePatternPickerRuntime() {
  if (!pattern_picker_.active) return;
  withRuntimeAudioGuard([&]() {
    if (mini_acid_.songModeEnabled()) mini_acid_.setSongMode(false);
    mini_acid_.set303BankIndex(0, pattern_picker_.savedSynthBank[0]);
    mini_acid_.set303PatternIndex(0, pattern_picker_.savedSynthPattern[0]);
    mini_acid_.set303BankIndex(1, pattern_picker_.savedSynthBank[1]);
    mini_acid_.set303PatternIndex(1, pattern_picker_.savedSynthPattern[1]);
    mini_acid_.setDrumBankIndex(pattern_picker_.savedDrumBank);
    mini_acid_.setDrumPatternIndex(pattern_picker_.savedDrumPattern);
    mini_acid_.setSongPosition(pattern_picker_.savedSongPosition);
    if (pattern_picker_.savedSongMode) mini_acid_.setSongMode(true);
  });
}

void SongPage::showPatternPickerStatus() {
  if (!pattern_picker_.active) return;
  char address[12];
  formatGlobalPatternAddress(
      address, sizeof(address), pattern_picker_.candidateGlobal);
  char status[64];
  if (pattern_picker_.mode == PatternPickerMode::Generate) {
    std::snprintf(status, sizeof(status),
                  "PICK GEN %s %s T%02u G:REROLL TAB ENTER ESC",
                  pickerTrackName(pattern_picker_.track), address,
                  static_cast<unsigned>(pattern_picker_.take % 100u));
  } else {
    std::snprintf(status, sizeof(status),
                  "PICK EXIST %s %s Q-I/B/<> TAB ENTER ESC",
                  pickerTrackName(pattern_picker_.track), address);
  }
  showToast(status, 60000);
}

void SongPage::closePatternPickerState() {
  pattern_picker_ = PatternPickerState{};
}

bool SongPage::selectExistingPickerPattern(int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  if (!address.valid() || address.page != mini_acid_.currentPageIndex() ||
      !pickerTrackHasMaterial(globalPattern)) {
    showToast("EMPTY PATTERN", 700);
    return false;
  }
  pattern_picker_.page = address.page;
  pattern_picker_.browseBank = address.bank;
  pattern_picker_.browseSlot = address.slot;
  pattern_picker_.candidateGlobal = globalPattern;
  if (!applyPatternPickerPreview(globalPattern)) return false;
  showPatternPickerStatus();
  return true;
}

bool SongPage::openPatternPicker() {
  if (pattern_picker_.active) return true;
  if (has_selection_) {
    showToast("PICKER: SINGLE CELL", 1000);
    return true;
  }
  if (mini_acid_.isPageLoading() || mini_acid_.targetPageIndex() >= 0) {
    showToast("PICKER: PAGE BUSY", 1000);
    return true;
  }

  bool valid = false;
  const SongTrack track = trackForColumn(cursorTrack(), valid);
  const uint8_t trackMask = valid
      ? SongPatternMaterializer::maskForTrack(track)
      : 0;
  if (!valid || trackMask == 0) {
    showToast("PICKER: TRACK UNAVAILABLE", 1000);
    return true;
  }

  pattern_picker_ = PatternPickerState{};
  pattern_picker_.active = true;
  pattern_picker_.row = cursorRow();
  pattern_picker_.songSlot = mini_acid_.activeSongSlot();
  pattern_picker_.track = track;
  pattern_picker_.trackMask = trackMask;
  pattern_picker_.page = mini_acid_.currentPageIndex();
  pattern_picker_.originalReference =
      mini_acid_.songPatternAt(pattern_picker_.row, track);
  pattern_picker_.savedSongMode = mini_acid_.songModeEnabled();
  pattern_picker_.savedSongPosition = mini_acid_.currentSongPosition();

  if (pattern_picker_.savedSongMode) {
    withRuntimeAudioGuard([&]() { mini_acid_.setSongMode(false); });
  }
  pattern_picker_.savedSynthPattern[0] = mini_acid_.current303PatternIndex(0);
  pattern_picker_.savedSynthPattern[1] = mini_acid_.current303PatternIndex(1);
  pattern_picker_.savedSynthBank[0] = mini_acid_.current303BankIndex(0);
  pattern_picker_.savedSynthBank[1] = mini_acid_.current303BankIndex(1);
  pattern_picker_.savedDrumPattern = mini_acid_.currentDrumPatternIndex();
  pattern_picker_.savedDrumBank = mini_acid_.currentDrumBankIndex();

  const PatternAddress original =
      patternAddressFromGlobal(pattern_picker_.originalReference);
  if (original.valid() && original.page == pattern_picker_.page &&
      pickerTrackHasMaterial(pattern_picker_.originalReference)) {
    selectExistingPickerPattern(pattern_picker_.originalReference);
    return true;
  }

  int preferredLocal = assignment_bank_index_ * Bank<SynthPattern>::kPatterns;
  if (track == SongTrack::SynthA) {
    preferredLocal = mini_acid_.current303BankIndex(0) *
        Bank<SynthPattern>::kPatterns + mini_acid_.current303PatternIndex(0);
  } else if (track == SongTrack::SynthB) {
    preferredLocal = mini_acid_.current303BankIndex(1) *
        Bank<SynthPattern>::kPatterns + mini_acid_.current303PatternIndex(1);
  } else if (track == SongTrack::Drums) {
    preferredLocal = mini_acid_.currentDrumBankIndex() *
        Bank<SynthPattern>::kPatterns + mini_acid_.currentDrumPatternIndex();
  }
  const int first = findExistingPickerPattern(preferredLocal, 1);
  if (first >= 0) selectExistingPickerPattern(first);
  else showPatternPickerStatus();
  return true;
}

bool SongPage::generatePatternPickerCandidate() {
  if (!pattern_picker_.active ||
      pattern_picker_.mode != PatternPickerMode::Generate) {
    return false;
  }
  if (mini_acid_.isPageLoading() || mini_acid_.targetPageIndex() >= 0) {
    showToast("PICKER: PAGE BUSY", 900);
    return false;
  }
  if (pattern_picker_.lease.isActive() &&
      pattern_picker_.page != mini_acid_.currentPageIndex()) {
    showToast("PICKER: LEASE PAGE PIN", 1200);
    return false;
  }
  pattern_picker_.page = mini_acid_.currentPageIndex();

  Scene& scene = mini_acid_.sceneManager().currentScene();
  int preferredLocal = pattern_picker_.browseBank * Bank<SynthPattern>::kPatterns +
      pattern_picker_.browseSlot;
  auto& owner = PhrasePatternLease::patternLeaseOwner();
  const PhrasePatternLease::AcquireResult acquired = owner.acquire(
      scene,
      pattern_picker_.page,
      1,
      pattern_picker_.trackMask,
      pattern_picker_.lease,
      preferredLocal);
  if (!acquired) {
    showToast(PhrasePatternLease::statusText(acquired.status), 1200);
    return false;
  }

  const int globalPattern = pattern_picker_.lease.globalPattern[0];
  const int localSlot = pickerLocalSlot(globalPattern);
  if (localSlot < 0) {
    showToast("PICKER: INVALID LEASE", 1100);
    return false;
  }

  SongPatternCandidate::Candidate candidate{};
  if (!SongPatternCandidate::produce(
          mini_acid_, pattern_picker_.track, pattern_picker_.row,
          pattern_picker_.page, localSlot, gen_mode_, candidate)) {
    showToast("GENERATION FAILED", 1100);
    return false;
  }

  bool wrote = false;
  withRuntimeAudioGuard([&]() {
    wrote = SongPatternCandidate::writeToLeasedAddress(
        scene, pattern_picker_.track, globalPattern, candidate);
  });
  if (!wrote) {
    showToast("PICKER: WRITE FAILED", 1100);
    return false;
  }

  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  pattern_picker_.candidateGlobal = globalPattern;
  pattern_picker_.browseBank = address.bank;
  pattern_picker_.browseSlot = address.slot;
  ++pattern_picker_.take;
  applyPatternPickerPreview(globalPattern);
  showPatternPickerStatus();
  return true;
}

bool SongPage::enterPatternPickerGenerateMode() {
  if (!pattern_picker_.active) return false;
  pattern_picker_.mode = PatternPickerMode::Generate;
  pattern_picker_.candidateGlobal = -1;
  pattern_picker_.take = 0;
  return generatePatternPickerCandidate();
}

bool SongPage::acceptPatternPicker() {
  if (!pattern_picker_.active || pattern_picker_.candidateGlobal < 0) {
    showToast("PICKER: NO CANDIDATE", 900);
    return false;
  }
  if (mini_acid_.activeSongSlot() != pattern_picker_.songSlot) {
    showToast("PICKER: SONG TARGET CHANGED", 1200);
    return false;
  }
  const PatternAddress address =
      patternAddressFromGlobal(pattern_picker_.candidateGlobal);
  if (!address.valid() || address.page != mini_acid_.currentPageIndex()) {
    showToast("PICKER: PAGE CHANGED", 1000);
    return false;
  }

  const int candidate = pattern_picker_.candidateGlobal;
  const int row = pattern_picker_.row;
  const SongTrack track = pattern_picker_.track;
  const PatternPickerMode mode = pattern_picker_.mode;
  restorePatternPickerRuntime();

  if (mode == PatternPickerMode::Existing) {
    if (candidate == pattern_picker_.originalReference) {
      closePatternPickerState();
      showToast("PICKER: NO CHANGE", 700);
      return true;
    }
    const bool committed = commitSongMutation([&](Song& song) {
      GroovePuterUndo::SongEdit::setPattern(song, row, track, candidate);
    });
    if (!committed) {
      enterPatternPickerAuditionRuntime();
      applyPatternPickerPreview(candidate);
      showToast("PICKER: ACCEPT FAILED", 1000);
      return false;
    }
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      withRuntimeAudioGuard([&]() { mini_acid_.setSongPosition(row); });
    }
    char label[12];
    formatGlobalPatternAddress(label, sizeof(label), candidate);
    closePatternPickerState();
    char message[32];
    std::snprintf(message, sizeof(message), "PICK ACCEPT %s", label);
    showToast(message, 900);
    return true;
  }

  auto& owner = PhrasePatternLease::patternLeaseOwner();
  PhrasePatternLease::PreparedPersistentTransfer prepared{};
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhrasePatternLease::LeaseStatus preparedStatus =
      owner.preparePersistentTransfer(
          scene,
          mini_acid_.currentPageIndex(),
          pattern_picker_.lease,
          PhrasePatternLease::PersistentClass::SongGenerated,
          prepared);
  if (preparedStatus != PhrasePatternLease::LeaseStatus::Ok) {
    enterPatternPickerAuditionRuntime();
    applyPatternPickerPreview(candidate);
    showToast(PhrasePatternLease::statusText(preparedStatus), 1200);
    return false;
  }

  const bool committed = commitSongMutation([&](Song& song) {
    GroovePuterUndo::SongEdit::setPattern(song, row, track, candidate);
  });
  if (!committed) {
    // prepare classified only lease-owned backing. The active lease remains the
    // rollback owner; Esc can still discard the candidate exactly.
    enterPatternPickerAuditionRuntime();
    applyPatternPickerPreview(candidate);
    showPatternPickerStatus();
    return false;
  }

  const PhrasePatternLease::LeaseStatus completedStatus =
      owner.completePersistentTransfer(pattern_picker_.lease, prepared);
  if (completedStatus != PhrasePatternLease::LeaseStatus::Ok) {
    // A valid synchronous prepare -> Song commit -> complete sequence is
    // bookkeeping-only and must not reach this branch.
    showToast("LEASE TRANSFER FAULT", 1800);
    return false;
  }

  if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
    withRuntimeAudioGuard([&]() { mini_acid_.setSongPosition(row); });
  }
  char label[12];
  formatGlobalPatternAddress(label, sizeof(label), candidate);
  closePatternPickerState();
  char message[32];
  std::snprintf(message, sizeof(message), "PICK GEN ACCEPT %s", label);
  showToast(message, 1000);
  return true;
}

bool SongPage::discardPatternPicker() {
  if (!pattern_picker_.active) return true;
  restorePatternPickerRuntime();
  if (pattern_picker_.lease.isActive()) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    const PhrasePatternLease::LeaseStatus status =
        PhrasePatternLease::patternLeaseOwner().discard(
            scene, mini_acid_.currentPageIndex(), pattern_picker_.lease);
    if (status != PhrasePatternLease::LeaseStatus::Ok) {
      showToast(PhrasePatternLease::statusText(status), 1200);
      enterPatternPickerAuditionRuntime();
      if (pattern_picker_.candidateGlobal >= 0) {
        applyPatternPickerPreview(pattern_picker_.candidateGlobal);
      }
      return false;
    }
  }
  closePatternPickerState();
  showToast("PICKER DISCARDED", 700);
  return true;
}

void SongPage::onExit() {
  if (pattern_picker_.active) {
    (void)discardPatternPicker();
  }
}

bool SongPage::handlePatternPickerEvent(UIEvent& ui_event) {
  if (!pattern_picker_.active) return false;

  // The picker is modal. Application events such as Undo/Copy/Paste are
  // consumed here so no Song mutation can occur behind the audition overlay.
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return true;

  if (!pattern_picker_.lease.isActive() && !mini_acid_.isPageLoading() &&
      mini_acid_.currentPageIndex() != pattern_picker_.page) {
    pattern_picker_.page = mini_acid_.currentPageIndex();
    pattern_picker_.candidateGlobal = -1;
    if (pattern_picker_.mode == PatternPickerMode::Existing) {
      const int first = findExistingPickerPattern(
          pattern_picker_.browseBank * Bank<SynthPattern>::kPatterns +
              pattern_picker_.browseSlot,
          1);
      if (first >= 0) selectExistingPickerPattern(first);
    }
  }

  const char key = normalizedSongR4Key(ui_event);
  const int nav = UIInput::navCode(ui_event);
  const bool escape = ui_event.scancode == GROOVEPUTER_ESCAPE ||
      ui_event.key == 0x1B || ui_event.key == '`' || ui_event.key == '~';
  if (escape) {
    (void)discardPatternPicker();
    return true;
  }
  if (ui_event.key == '\n' || ui_event.key == '\r') {
    (void)acceptPatternPicker();
    return true;
  }

  if (ui_event.key == '\t' || ui_event.scancode == GROOVEPUTER_TAB) {
    if (pattern_picker_.mode == PatternPickerMode::Existing) {
      (void)enterPatternPickerGenerateMode();
    } else {
      if (pattern_picker_.lease.isActive()) {
        Scene& scene = mini_acid_.sceneManager().currentScene();
        const PhrasePatternLease::LeaseStatus status =
            PhrasePatternLease::patternLeaseOwner().discard(
                scene, mini_acid_.currentPageIndex(), pattern_picker_.lease);
        if (status != PhrasePatternLease::LeaseStatus::Ok) {
          showToast(PhrasePatternLease::statusText(status), 1200);
          return true;
        }
      }
      pattern_picker_.mode = PatternPickerMode::Existing;
      pattern_picker_.take = 0;
      const int first = findExistingPickerPattern(
          pattern_picker_.browseBank * Bank<SynthPattern>::kPatterns +
              pattern_picker_.browseSlot,
          1);
      pattern_picker_.candidateGlobal = -1;
      if (first >= 0) selectExistingPickerPattern(first);
      else showPatternPickerStatus();
    }
    return true;
  }

  if (pattern_picker_.mode == PatternPickerMode::Generate) {
    if (key == 'g' || ui_event.scancode == GROOVEPUTER_G) {
      (void)generatePatternPickerCandidate();
      return true;
    }
    if (key == 'b' || ui_event.scancode == GROOVEPUTER_B) {
      showToast("PICKER: LEASE ADDRESS PINNED", 900);
      return true;
    }
    // All remaining keys are consumed; legacy Song G/Q/B/Undo cannot fall
    // through while a generated candidate owns temporary storage.
    return true;
  }

  int directSlot = patternIndexFromKey(key);
  if (directSlot < 0) directSlot = scancodeToPatternIndex(ui_event.scancode);
  if (directSlot >= 0 && !ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    const int global = songPatternFromPageBankIndex(
        mini_acid_.currentPageIndex(), pattern_picker_.browseBank, directSlot);
    (void)selectExistingPickerPattern(global);
    return true;
  }

  if (!ui_event.ctrl && !ui_event.alt &&
      (key == 'b' || ui_event.scancode == GROOVEPUTER_B)) {
    pattern_picker_.browseBank = pattern_picker_.browseBank == 0 ? 1 : 0;
    const int sameSlot = songPatternFromPageBankIndex(
        mini_acid_.currentPageIndex(), pattern_picker_.browseBank,
        pattern_picker_.browseSlot);
    if (!selectExistingPickerPattern(sameSlot)) {
      const int first = findExistingPickerPattern(
          pattern_picker_.browseBank * Bank<SynthPattern>::kPatterns,
          1,
          pattern_picker_.browseBank);
      if (first >= 0) selectExistingPickerPattern(first);
      else showPatternPickerStatus();
    }
    return true;
  }

  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT ||
      nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    const int direction =
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_UP) ? -1 : 1;
    int local = pickerLocalSlot(pattern_picker_.candidateGlobal);
    if (local < 0) {
      local = pattern_picker_.browseBank * Bank<SynthPattern>::kPatterns +
          pattern_picker_.browseSlot;
    }
    local += direction;
    while (local < 0) local += kPatternsPerPage;
    while (local >= kPatternsPerPage) local -= kPatternsPerPage;
    const int next = findExistingPickerPattern(local, direction);
    if (next >= 0) selectExistingPickerPattern(next);
    else showToast("NO EXISTING PATTERNS", 900);
    return true;
  }

  return true;
}

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

  if (pattern_picker_.active) {
    return handlePatternPickerEvent(ui_event);
  }

  if (songPatternPickerOpenGesture(ui_event)) {
    return openPatternPicker();
  }

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
