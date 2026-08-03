#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, path: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one anchor, found {count}: {old[:80]!r}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, path: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: regex anchor not unique: {pattern[:100]!r}")
    return updated


def patch_project_page() -> None:
    path = "src/ui/pages/project_page.cpp"
    text = read(path)

    text = regex_once(
        text,
        r"\nconstexpr size_t kMaxMidiDirsInUi = 24;.*?#endif\n",
        "\n",
        path,
    )

    replacement = r'''void ProjectPage::openImportMidiDialog() {
  GroovePuterUi::midiFileManager().open();
  midi_selected_path_.clear();
  dialog_type_ = DialogType::ImportMidi;
  dialog_focus_ = DialogFocus::List;
  selection_index_ = 0;
  scroll_offset_ = 0;
  midi_import_start_pattern_ = 0;
  midi_import_from_bar_ = 0;
  midi_import_length_bars_ = 16;
  midi_mask_a_ = 0;
  midi_mask_b_ = 0;
  midi_mask_d_ = 0;
  midi_import_append_ = false;
}

void ProjectPage::openMidiAdvanceDialog() {
  char selectedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
  if (!GroovePuterUi::midiFileManager().selectedFilePath(
          selectedPath, sizeof(selectedPath))) {
    UI::showToast("Select a MIDI file", 900);
    return;
  }

  midi_selected_path_ = selectedPath;
  dialog_type_ = DialogType::MidiAdvance;
  midi_advance_focus_ = MidiAdvanceFocus::Mode;
  midi_adv_scroll_ = 0;

  UI::showToast("Scanning MIDI...");
  MidiImporter importer(mini_acid_);
  midi_scan_ = importer.scanFile(midi_selected_path_);
  if (midi_scan_.valid) {
    autoRouteMidi();
    if (midi_scan_.estimatedBars > 0) {
      midi_import_length_bars_ = midi_scan_.estimatedBars;
    }
  }
}

void ProjectPage::autoRouteMidi()'''
    text = regex_once(
        text,
        r"void ProjectPage::refreshMidiFiles\(\) \{.*?void ProjectPage::autoRouteMidi\(\)",
        replacement,
        path,
    )
    text = replace_once(
        text,
        "  std::string path = midi_current_path_;",
        "  std::string path = midi_selected_path_;",
        path,
    )

    import_start = r'''bool ProjectPage::importMidiAtSelection() {
  if (midi_selected_path_.empty()) {
    UI::showToast("Select a MIDI file", 900);
    dialog_type_ = DialogType::ImportMidi;
    return true;
  }

  if ((midi_mask_a_ | midi_mask_b_ | midi_mask_d_) == 0)'''
    text = regex_once(
        text,
        r"bool ProjectPage::importMidiAtSelection\(\) \{.*?  if \(\(midi_mask_a_ \| midi_mask_b_ \| midi_mask_d_\) == 0\)",
        import_start,
        path,
    )
    text = regex_once(
        text,
        r"\n  const std::string path = midi_current_path_ \+ \"/\" \+ midi_files_\[fileIdx\];",
        "\n  const std::string path = midi_selected_path_;",
        path,
    )

    delete_function = r'''bool ProjectPage::deleteSelectionInDialog() {
  if (dialog_focus_ != DialogFocus::List || dialog_type_ != DialogType::Load) {
    return true;
  }
  if (scenes_.empty()) return true;
  if (selection_index_ < 0 || selection_index_ >= static_cast<int>(scenes_.size())) {
    return true;
  }

  const std::string name = scenes_[selection_index_];
  const std::string path = "/scenes/" + name + ".json";
  const std::string autoPath = "/scenes/" + name + ".auto.json";
  const bool removed = SD.remove(path.c_str());
  SD.remove(autoPath.c_str());
  if (removed) {
    UI::showToast("Scene deleted");
    refreshScenes();
    if (selection_index_ >= static_cast<int>(scenes_.size())) {
      selection_index_ = static_cast<int>(scenes_.size()) - 1;
    }
    if (selection_index_ < 0) selection_index_ = 0;
    ensureSelectionVisible(10);
  } else {
    UI::showToast("Delete failed");
  }
  return true;
}

void ProjectPage::closeDialog()'''
    text = regex_once(
        text,
        r"bool ProjectPage::deleteSelectionInDialog\(\) \{.*?void ProjectPage::closeDialog\(\)",
        delete_function,
        path,
    )

    ensure_function = r'''void ProjectPage::ensureSelectionVisible(int visibleRows) {
  if (visibleRows < 1) visibleRows = 1;
  const int listCount = static_cast<int>(scenes_.size());
  if (listCount <= 0) {
    scroll_offset_ = 0;
    selection_index_ = 0;
    return;
  }
  const int maxIdx = listCount - 1;
  selection_index_ = std::max(0, std::min(selection_index_, maxIdx));
  if (scroll_offset_ > selection_index_) scroll_offset_ = selection_index_;
  if (selection_index_ >= scroll_offset_ + visibleRows) {
    scroll_offset_ = selection_index_ - visibleRows + 1;
  }
  const int maxScroll = std::max(0, maxIdx - visibleRows + 1);
  scroll_offset_ = std::max(0, std::min(scroll_offset_, maxScroll));
}

void ProjectPage::ensureMainFocusVisible'''
    text = regex_once(
        text,
        r"void ProjectPage::ensureSelectionVisible\(int visibleRows\) \{.*?void ProjectPage::ensureMainFocusVisible",
        ensure_function,
        path,
    )

    input_block = r'''    if (dialog_type_ == DialogType::ImportMidi) {
        if (ui_event.key == '\t') {
            openMidiAdvanceDialog();
            return true;
        }
        char activatedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
        const auto result = GroovePuterUi::midiFileManager().handleEvent(
            ui_event, activatedPath, sizeof(activatedPath));
        if (result == GroovePuterUi::MidiFileManager::EventResult::FileActivated) {
            midi_selected_path_ = activatedPath;
            openMidiAdvanceDialog();
            return true;
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            closeDialog();
            return true;
        }
        return true;
    }

    if (dialog_type_ == DialogType::Load) {
        if (ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == '\b') {
            closeDialog();
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_LEFT) {
            dialog_focus_ = DialogFocus::List;
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_RIGHT) {
            dialog_focus_ = DialogFocus::Cancel;
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_UP && dialog_focus_ == DialogFocus::List) {
            moveSelection(-1);
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_DOWN && dialog_focus_ == DialogFocus::List) {
            moveSelection(1);
            return true;
        }
        if (ui_event.key == 'x' || ui_event.key == 'X') {
            return deleteSelectionInDialog();
        }
        if (ui_event.key == '\n' || ui_event.key == '\r') {
            if (dialog_focus_ == DialogFocus::Cancel) {
                closeDialog();
                return true;
            }
            return loadSceneAtSelection();
        }
        return true;
    }

    if (dialog_type_ == DialogType::MidiAdvance)'''
    text = regex_once(
        text,
        r"    if \(dialog_type_ == DialogType::Load \|\| dialog_type_ == DialogType::ImportMidi\) \{.*?    if \(dialog_type_ == DialogType::MidiAdvance\)",
        input_block,
        path,
    )

    dialog_draw = r'''  if (dialog_type_ == DialogType::ImportMidi) {
    GroovePuterUi::midiFileManager().draw(gfx, Layout::CONTENT, "IMPORT");
    return;
  }

  if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::SaveAs) {
    const int w = Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X;
    const int h = Layout::CONTENT.h - 2 * Layout::CONTENT_PAD_Y;
    const int yStart = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
    int dialogW = w - 16;
    if (dialogW < 80) dialogW = w - 4;
    int dialogH = h - 16;
    if (dialogH < 70) dialogH = h - 4;
    const int dialogX = x + (w - dialogW) / 2;
    const int dialogY = yStart + (h - dialogH) / 2;

    gfx.fillRect(dialogX, dialogY, dialogW, dialogH, COLOR_DARKER);
    gfx.drawRect(dialogX, dialogY, dialogW, dialogH, COLOR_ACCENT);

    if (dialog_type_ == DialogType::Load) {
      const int headerH = line_h + 4;
      const int cancelH = line_h + 8;
      const int listY = dialogY + headerH + 2;
      const int listH = dialogH - headerH - cancelH - 10;
      const int rowH = line_h + 3;
      const int visibleRows = std::max(1, listH / rowH);
      ensureSelectionVisible(visibleRows);
      gfx.setTextColor(COLOR_WHITE);
      gfx.drawText(dialogX + 4, dialogY + 2, "Load Scene");

      if (scenes_.empty()) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(dialogX + 6, listY + 2, "No files");
      } else {
        const int rows = std::min(visibleRows,
                                  static_cast<int>(scenes_.size()) - scroll_offset_);
        for (int row = 0; row < rows; ++row) {
          const int index = scroll_offset_ + row;
          const int rowY = listY + row * rowH;
          const bool selected = index == selection_index_;
          if (selected) {
            gfx.fillRect(dialogX + 2, rowY, dialogW - 4, rowH, COLOR_PANEL);
            gfx.drawRect(dialogX + 2, rowY, dialogW - 4, rowH, COLOR_ACCENT);
          }
          gfx.setTextColor(selected ? COLOR_WHITE : COLOR_LABEL);
          Widgets::drawClippedText(gfx, dialogX + 6, rowY + 1,
                                   dialogW - 12, scenes_[index].c_str());
        }
      }

      const int buttonWidth = 60;
      const int buttonX = dialogX + (dialogW - buttonWidth) / 2;
      const int buttonY = dialogY + dialogH - cancelH - 4;
      const bool focused = dialog_focus_ == DialogFocus::Cancel;
      gfx.fillRect(buttonX, buttonY, buttonWidth, cancelH, COLOR_PANEL);
      gfx.drawRect(buttonX, buttonY, buttonWidth, cancelH,
                   focused ? COLOR_ACCENT : COLOR_LABEL);
      gfx.setTextColor(focused ? COLOR_WHITE : COLOR_LABEL);
      gfx.drawText(buttonX + (buttonWidth - textWidth(gfx, "Cancel")) / 2,
                   buttonY + (cancelH - line_h) / 2, "Cancel");
      return;
    }

    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialogX + 4, dialogY + 2, "Save Scene");
    const int inputH = line_h + 8;
    const int inputY = dialogY + line_h + 6;
    gfx.fillRect(dialogX + 4, inputY, dialogW - 8, inputH, COLOR_PANEL);
    const bool inputFocused = save_dialog_focus_ == SaveDialogFocus::Input;
    gfx.drawRect(dialogX + 4, inputY, dialogW - 8, inputH,
                 inputFocused ? COLOR_ACCENT : COLOR_LABEL);
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialogX + 8, inputY + (inputH - line_h) / 2,
                 save_name_.c_str());

    const char* buttons[] = {"RND", "SAVE", "ESC"};
    const SaveDialogFocus focuses[] = {
        SaveDialogFocus::Randomize,
        SaveDialogFocus::Save,
        SaveDialogFocus::Cancel,
    };
    const int buttonWidth = (dialogW - 16) / 3;
    for (int index = 0; index < 3; ++index) {
      const int buttonX = dialogX + 4 + index * (buttonWidth + 4);
      const int buttonY = inputY + inputH + 6;
      const bool focused = save_dialog_focus_ == focuses[index];
      gfx.fillRect(buttonX, buttonY, buttonWidth, line_h + 8, COLOR_PANEL);
      gfx.drawRect(buttonX, buttonY, buttonWidth, line_h + 8,
                   focused ? COLOR_ACCENT : COLOR_LABEL);
      gfx.setTextColor(focused ? COLOR_WHITE : COLOR_LABEL);
      gfx.drawText(buttonX + (buttonWidth - textWidth(gfx, buttons[index])) / 2,
                   buttonY + 4, buttons[index]);
    }
    return;
  }

  // Main Page Drawing'''
    text = regex_once(
        text,
        r"  if \(dialog_type_ != DialogType::None\) \{.*?  // Main Page Drawing",
        dialog_draw,
        path,
    )

    for forbidden in ("midi_dirs_", "midi_files_", "midi_current_path_", "refreshMidiFiles"):
        if forbidden in text:
            raise RuntimeError(f"{path}: duplicate MIDI browser symbol remains: {forbidden}")
    write(path, text)


def patch_smf_player_page() -> None:
    path = "src/ui/pages/smf_player_page.cpp"
    text = read(path)

    text = regex_once(
        text,
        r"\nconst char\* browserBasename\(.*?\nvoid formatMidiNote",
        "\nvoid formatMidiNote",
        path,
    )
    text = replace_once(
        text,
        "    if (entryCount() == 0) refreshFiles();",
        "    if (browserVisible_) GroovePuterUi::midiFileManager().open();",
        path,
    )

    load_function = r'''bool SmfPlayerPage::loadMidiPath(const char* path) {
    if (!path || path[0] == '\0') {
        UI::showToast("MIDI entry unavailable", 900);
        return true;
    }

    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }
    const SmfPlayerSnapshot playerState = player_->snapshot();
    if (!player_->requestLoad(path)) {
        UI::showToast("Player queue busy", 1000);
        return true;
    }
    browserVisible_ = false;
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    const bool followSeqtrak = clock.source == TransportClockSource::SeqtrakExternal;
    UI::showToast(playerState.tempoMode == SmfTempoMode::Project
                      ? (followSeqtrak
                             ? (clock.externalFollowEnabled
                                    ? "LOADING / SPACE ARM / SEQ PLAY"
                                    : "LOADING / FOLLOW OFF")
                             : "LOADING / G THEN SPACE")
                      : "LOADING / SPACE TO PLAY",
                  1000);
    return true;
}

bool SmfPlayerPage::togglePlayerTransport()'''
    text = regex_once(
        text,
        r"void SmfPlayerPage::refreshFiles\(\) \{.*?bool SmfPlayerPage::togglePlayerTransport\(\)",
        load_function,
        path,
    )

    browser_input = r'''    if (browserVisible_) {
        char activatedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
        const auto result = GroovePuterUi::midiFileManager().handleEvent(
            event, activatedPath, sizeof(activatedPath));
        if (result == GroovePuterUi::MidiFileManager::EventResult::FileActivated) {
            return loadMidiPath(activatedPath);
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                if (state.state != SmfPlayerState::Unloaded &&
                    state.state != SmfPlayerState::Error) {
                    browserVisible_ = false;
                }
            }
            return true;
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::Consumed) {
            return true;
        }
        if (event.key == 'm' || event.key == 'M') {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool queued = player_->toggleRouting();
                UI::showToast(queued
                                  ? (state.rawRouting ? "ROUTE: SEQTRAK SAFE" : "ROUTE: RAW")
                                  : "MIDI PLAYER BUSY",
                              850);
            }
            return true;
        }
        if (event.key == 't' || event.key == 'T') {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool toProject = state.tempoMode == SmfTempoMode::Original;
                const bool queued = player_->toggleTempoMode();
                const bool followSeqtrak = transportClockRuntime().source() ==
                    TransportClockSource::SeqtrakExternal;
                UI::showToast(queued
                                  ? (toProject
                                         ? (followSeqtrak
                                                ? "TEMPO: SEQ MASTER"
                                                : "TEMPO: GP MASTER > USB")
                                         : "TEMPO: FILE ORIGINAL")
                                  : "MIDI PLAYER BUSY",
                              900);
            }
            return true;
        }
        return false;
    }

    if (!player_) return false;'''
    text = regex_once(
        text,
        r"    if \(browserVisible_\) \{.*?    if \(!player_\) return false;",
        browser_input,
        path,
    )

    text = replace_once(
        text,
        "        refreshFiles();\n        return true;",
        "        GroovePuterUi::midiFileManager().open();\n        return true;",
        path,
    )

    draw_browser = r'''void SmfPlayerPage::drawBrowser(IGfx& gfx) {
    GroovePuterUi::midiFileManager().draw(gfx, Layout::CONTENT, "PLAYER");
}

void SmfPlayerPage::drawNowPlaying'''
    text = regex_once(
        text,
        r"void SmfPlayerPage::drawBrowser\(IGfx& gfx\) \{.*?void SmfPlayerPage::drawNowPlaying",
        draw_browser,
        path,
    )

    text = replace_once(
        text,
        '''        UI::drawStandardFooter(gfx, "UP/DN Select Enter Load",
                               seqMaster ? "C Master G Follow T Tempo"
                                         : "C Master Space MIDI T Tempo");''',
        '''        UI::drawStandardFooter(gfx, "ENT Open R Name X Delete",
                               seqMaster ? "F Refresh C Master G Follow"
                                         : "F Refresh C Master T Tempo");''',
        path,
    )
    text = regex_once(
        text,
        r"\nvoid SmfPlayerPage::ensureSelectionVisible\(int visibleRows\) \{.*?\n\}",
        "",
        path,
    )

    for forbidden in (
        "refreshFiles", "fillVisibleEntries", "resolveEntry", "navigateIntoDir",
        "navigateUpDir", "entryCount()", "displayName(index)", "currentPath_",
        "directoryCount_", "fileCount_", "browserRows_",
    ):
        if forbidden in text:
            raise RuntimeError(f"{path}: duplicate MIDI browser symbol remains: {forbidden}")
    write(path, text)


def patch_service_path_contract() -> None:
    path = "src/midi/smf_player_service.h"
    text = read(path)
    text = replace_once(text, "#include <cstdint>\n", "#include <cstddef>\n#include <cstdint>\n", path)
    text = replace_once(
        text,
        "    virtual SmfPlayerSnapshot snapshot() const = 0;\n    virtual SmfChannelInspectorSnapshot channelInspector() const = 0;",
        '''    virtual SmfPlayerSnapshot snapshot() const = 0;
    virtual SmfChannelInspectorSnapshot channelInspector() const = 0;
    virtual bool currentFilePath(char* output, std::size_t outputSize) const {
        if (output && outputSize > 0) output[0] = '\\0';
        return false;
    }''',
        path,
    )
    write(path, text)

    path = "src/platform/cardputer_smf_player.h"
    text = read(path)
    text = replace_once(
        text,
        "    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;\n    GroovePuterMidi::SmfChannelInspectorSnapshot channelInspector() const override;",
        '''    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;
    GroovePuterMidi::SmfChannelInspectorSnapshot channelInspector() const override;
    bool currentFilePath(char* output, std::size_t outputSize) const override;''',
        path,
    )
    text = replace_once(
        text,
        "    GroovePuterMidi::SmfPlayerSnapshot snapshot_{};\n    GroovePuterMidi::SmfChannelInspectorSnapshot channelInspector_{};",
        '''    GroovePuterMidi::SmfPlayerSnapshot snapshot_{};
    GroovePuterMidi::SmfChannelInspectorSnapshot channelInspector_{};
    char loadedPath_[kPathBytes]{};''',
        path,
    )
    write(path, text)

    path = "src/platform/cardputer_smf_player.cpp"
    text = read(path)
    text = replace_once(
        text,
        '''SmfChannelInspectorSnapshot CardputerSmfPlayerService::channelInspector() const {
    portENTER_CRITICAL(&snapshotMux_);
    const SmfChannelInspectorSnapshot copy = channelInspector_;
    portEXIT_CRITICAL(&snapshotMux_);
    return copy;
}
''',
        '''SmfChannelInspectorSnapshot CardputerSmfPlayerService::channelInspector() const {
    portENTER_CRITICAL(&snapshotMux_);
    const SmfChannelInspectorSnapshot copy = channelInspector_;
    portEXIT_CRITICAL(&snapshotMux_);
    return copy;
}

bool CardputerSmfPlayerService::currentFilePath(
        char* output,
        std::size_t outputSize) const {
    if (!output || outputSize == 0) return false;
    portENTER_CRITICAL(&snapshotMux_);
    copyText(output, outputSize, loadedPath_);
    const bool available = loadedPath_[0] != '\\0';
    portEXIT_CRITICAL(&snapshotMux_);
    return available;
}
''',
        path,
    )
    text = replace_once(
        text,
        '''    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = SmfChannelInspectorSnapshot{};
    portEXIT_CRITICAL(&snapshotMux_);
''',
        '''    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = SmfChannelInspectorSnapshot{};
    loadedPath_[0] = '\\0';
    portEXIT_CRITICAL(&snapshotMux_);
''',
        path,
    )
    text = replace_once(
        text,
        "    copyText(snapshot_.filename, sizeof(snapshot_.filename), basename(path));",
        '''    copyText(snapshot_.filename, sizeof(snapshot_.filename), basename(path));
    copyText(loadedPath_, sizeof(loadedPath_), path);''',
        path,
    )
    write(path, text)

    path = "src/ui/midi_file_manager.cpp"
    text = read(path)
    old = r'''    const GroovePuterMidi::SmfPlayerSnapshot snapshot = player->snapshot();
    if (snapshot.state == GroovePuterMidi::SmfPlayerState::Unloaded ||
        snapshot.state == GroovePuterMidi::SmfPlayerState::Error ||
        snapshot.filename[0] == '\0') {
        return false;
    }
    return equalIgnoreCase(entry->name, basenameOf(snapshot.filename));'''
    new = r'''    char selectedPath[kPathBytes]{};
    char loadedPath[kPathBytes]{};
    if (!buildPathForEntry(*entry, selectedPath, sizeof(selectedPath)) ||
        !player->currentFilePath(loadedPath, sizeof(loadedPath))) {
        return false;
    }
    return equalIgnoreCase(selectedPath, loadedPath);'''
    text = replace_once(text, old, new, path)
    write(path, text)


def patch_build_and_tests() -> None:
    path = "platform_sdl/Makefile"
    text = read(path)
    text = replace_once(
        text,
        "\t../src/ui/ui_clipboard.cpp \\\n",
        "\t../src/ui/ui_clipboard.cpp \\\n\t../src/ui/midi_file_manager.cpp \\\n",
        path,
    )
    write(path, text)

    path = "tests/run_host_tests.sh"
    text = read(path)
    text = replace_once(
        text,
        'python3 "${ROOT_DIR}/tests/test_scene_revision_source_regressions.py"\n',
        'python3 "${ROOT_DIR}/tests/test_scene_revision_source_regressions.py"\n'
        'python3 "${ROOT_DIR}/tests/test_midi_file_manager_source_regressions.py"\n',
        path,
    )
    text += r'''

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_file_name_policy.cpp" \
  -o "${BUILD_DIR}/test_midi_file_name_policy"

"${BUILD_DIR}/test_midi_file_name_policy"
'''
    write(path, text)


def main() -> None:
    patch_project_page()
    patch_smf_player_page()
    patch_service_path_contract()
    patch_build_and_tests()


if __name__ == "__main__":
    main()
