#pragma once

#include <cstdint>
#include <cstring>

#include "../../scenes.h"
#include "phrase_types.h"

namespace PhraseCore {

inline int trackIndex(SongTrack track) {
  switch (track) {
    case SongTrack::SynthA: return 0;
    case SongTrack::SynthB: return 1;
    case SongTrack::Drums: return 2;
    case SongTrack::Voice: break;
  }
  return -1;
}

inline bool isValid(const PhraseSlot& slot) {
  const PhraseMetadata& metadata = slot.metadata;
  return (metadata.flags & kFlagValid) != 0 &&
         metadata.phraseId != kNoPhraseId &&
         isValidLength(metadata.lengthBars) &&
         isValidRole(metadata.role) &&
         isValidSource(metadata.source) &&
         metadata.storage != StorageMode::Empty &&
         isValidStorage(metadata.storage) &&
         isValidTrackMask(metadata.trackMask) &&
         metadata.sourceSongSlot <= 1 &&
         metadata.sourceStartRow < Song::kMaxPositions;
}

inline int16_t patternAt(const PhraseSlot& phrase, uint8_t bar,
                         SongTrack track) {
  const int index = trackIndex(track);
  if (!isValid(phrase) || bar >= phrase.metadata.lengthBars || index < 0) {
    return -1;
  }
  return phrase.patternRefs[bar][index];
}

inline SlotSummary summarize(const PhraseBank& bank, SlotId slotId) {
  SlotSummary summary{};
  summary.slot = slotId;
  const PhraseSlot* phrase = slotAt(bank, slotId);
  if (!phrase || !isValid(*phrase)) return summary;
  summary.valid = true;
  summary.phraseId = phrase->metadata.phraseId;
  summary.parentId = phrase->metadata.parentId;
  summary.lengthBars = phrase->metadata.lengthBars;
  summary.role = phrase->metadata.role;
  summary.source = phrase->metadata.source;
  summary.storage = phrase->metadata.storage;
  summary.trackMask = phrase->metadata.trackMask;
  summary.mutableBacking =
      (phrase->metadata.flags & kFlagMutableBacking) != 0;
  return summary;
}

inline Result clear(PhraseBank& bank, SlotId slotId) {
  Result result{};
  result.slot = slotId;
  PhraseSlot* phrase = slotAt(bank, slotId);
  if (!phrase) {
    result.error = Error::InvalidSlot;
    return result;
  }
  clearSlotValue(*phrase);
  return result;
}

inline Result captureSongRegion(PhraseBank& bank,
                                SlotId targetSlot,
                                const Song& song,
                                uint8_t sourceSongSlot,
                                uint8_t startRow,
                                uint8_t lengthBars,
                                Role role,
                                Source source,
                                uint8_t trackMask = kAllTracks,
                                uint8_t parentSlot = kNoSlot) {
  Result result{};
  result.slot = targetSlot;
  PhraseSlot* destination = slotAt(bank, targetSlot);
  if (!destination) {
    result.error = Error::InvalidSlot;
    return result;
  }
  if (!isValidLength(lengthBars)) {
    result.error = Error::InvalidLength;
    return result;
  }
  if (!isValidTrackMask(trackMask)) {
    result.error = Error::InvalidTrackMask;
    return result;
  }
  if (sourceSongSlot > 1) {
    result.error = Error::InvalidSongSlot;
    return result;
  }
  if (!isSongReferenceSource(source)) {
    result.error = Error::InvalidSource;
    return result;
  }
  if (!isValidRole(role)) {
    result.error = Error::InvalidRole;
    return result;
  }
  if (startRow >= Song::kMaxPositions ||
      static_cast<int>(startRow) + lengthBars > Song::kMaxPositions ||
      static_cast<int>(startRow) + lengthBars > song.length) {
    result.error = Error::RegionOutOfRange;
    return result;
  }

  uint16_t parentId = kNoPhraseId;
  if (parentSlot != kNoSlot) {
    if (parentSlot >= kSlotCount || !isValid(bank.slots[parentSlot])) {
      result.error = Error::InvalidParent;
      return result;
    }
    parentId = bank.slots[parentSlot].metadata.phraseId;
  } else if (source == Source::Derived) {
    result.error = Error::InvalidParent;
    return result;
  }

  PhraseSlot candidate{};
  clearSlotValue(candidate);
  bool hasMaterial = false;
  for (int bar = 0; bar < lengthBars; ++bar) {
    const SongPosition& position = song.positions[startRow + bar];
    for (int track = 0; track < kTrackCount; ++track) {
      if ((trackMask & maskForTrackIndex(track)) == 0) continue;
      const int16_t reference = position.patterns[track];
      candidate.patternRefs[bar][track] = reference;
      if (reference >= 0) hasMaterial = true;
    }
  }
  if (!hasMaterial) {
    result.error = Error::EmptyRegion;
    return result;
  }

  const uint16_t phraseId = nextPhraseId(bank);
  candidate.metadata.phraseId = phraseId;
  candidate.metadata.parentId = parentId;
  candidate.metadata.lengthBars = lengthBars;
  candidate.metadata.role = role;
  candidate.metadata.source = source;
  candidate.metadata.storage = StorageMode::ReferenceView;
  candidate.metadata.flags =
      kFlagValid | kFlagReferenceView | kFlagMutableBacking;
  candidate.metadata.sourceSongSlot = sourceSongSlot;
  candidate.metadata.sourceStartRow = startRow;
  candidate.metadata.trackMask = trackMask;

  *destination = candidate;
  bank.nextPhraseId = incrementPhraseId(phraseId);
  bank.version = kPersistenceVersion;
  result.phraseId = phraseId;
  return result;
}

inline Result deriveReferenceView(PhraseBank& bank,
                                  SlotId targetSlot,
                                  SlotId parentSlot,
                                  Role role) {
  Result result{};
  result.slot = targetSlot;
  PhraseSlot* destination = slotAt(bank, targetSlot);
  const PhraseSlot* parent = slotAt(bank, parentSlot);
  if (!destination) {
    result.error = Error::InvalidSlot;
    return result;
  }
  if (!parent || !isValid(*parent)) {
    result.error = Error::InvalidParent;
    return result;
  }
  if (!isValidRole(role)) {
    result.error = Error::InvalidRole;
    return result;
  }

  PhraseSlot candidate = *parent;
  const uint16_t phraseId = nextPhraseId(bank);
  candidate.metadata.phraseId = phraseId;
  candidate.metadata.parentId = parent->metadata.phraseId;
  candidate.metadata.role = role;
  candidate.metadata.source = Source::Derived;
  candidate.metadata.storage = StorageMode::ReferenceView;
  candidate.metadata.flags =
      kFlagValid | kFlagReferenceView | kFlagMutableBacking;

  *destination = candidate;
  bank.nextPhraseId = incrementPhraseId(phraseId);
  bank.version = kPersistenceVersion;
  result.phraseId = phraseId;
  return result;
}

inline Result writeToSong(const PhraseBank& bank,
                          SlotId sourceSlot,
                          Song& destination,
                          uint8_t startRow,
                          bool overwrite) {
  Result result{};
  result.slot = sourceSlot;
  const PhraseSlot* phrase = slotAt(bank, sourceSlot);
  if (!phrase || !isValid(*phrase)) {
    result.error = Error::InvalidPhrase;
    return result;
  }
  if (startRow >= Song::kMaxPositions ||
      static_cast<int>(startRow) + phrase->metadata.lengthBars >
          Song::kMaxPositions) {
    result.error = Error::RegionOutOfRange;
    return result;
  }

  if (!overwrite) {
    for (int bar = 0; bar < phrase->metadata.lengthBars; ++bar) {
      for (int track = 0; track < kTrackCount; ++track) {
        if ((phrase->metadata.trackMask & maskForTrackIndex(track)) == 0) {
          continue;
        }
        if (destination.positions[startRow + bar].patterns[track] >= 0) {
          result.error = Error::DestinationOccupied;
          return result;
        }
      }
    }
  }

  for (int bar = 0; bar < phrase->metadata.lengthBars; ++bar) {
    for (int track = 0; track < kTrackCount; ++track) {
      if ((phrase->metadata.trackMask & maskForTrackIndex(track)) == 0) {
        continue;
      }
      destination.positions[startRow + bar].patterns[track] =
          phrase->patternRefs[bar][track];
    }
  }
  const int requiredLength = startRow + phrase->metadata.lengthBars;
  if (destination.length < requiredLength) destination.length = requiredLength;
  result.phraseId = phrase->metadata.phraseId;
  return result;
}

inline uint16_t synthOccupancyMask(const SynthPattern& pattern,
                                   uint16_t& eventCount) {
  uint16_t mask = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) {
      mask |= static_cast<uint16_t>(1u << step);
      ++eventCount;
    }
  }
  return mask;
}

inline uint16_t drumOccupancyMask(const DrumPatternSet& pattern,
                                  uint16_t& eventCount) {
  uint16_t mask = 0;
  for (int step = 0; step < DrumPattern::kSteps; ++step) {
    bool occupied = false;
    for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
      if (pattern.voices[voice].steps[step].hit) {
        occupied = true;
        ++eventCount;
      }
    }
    if (occupied) mask |= static_cast<uint16_t>(1u << step);
  }
  return mask;
}

inline bool buildBarPreview(const PhraseSlot& phrase,
                            uint8_t bar,
                            const Scene& currentPageScene,
                            int currentPageIndex,
                            BarPreview& preview) {
  preview = BarPreview{};
  if (!isValid(phrase) || bar >= phrase.metadata.lengthBars ||
      currentPageIndex < 0 || currentPageIndex >= kMaxPages) {
    return false;
  }

  uint16_t eventCount = 0;
  for (int track = 0; track < kTrackCount; ++track) {
    const int16_t reference = phrase.patternRefs[bar][track];
    preview.patternRefs[track] = reference;
    if (reference < 0) continue;
    if (songPatternPage(reference) != currentPageIndex) {
      ++eventCount;
      continue;
    }
    const int bank = songPatternBank(reference);
    const int index = songPatternIndexInBank(reference);
    if (bank < 0 || bank >= kBankCount || index < 0 ||
        index >= Bank<SynthPattern>::kPatterns) {
      ++eventCount;
      continue;
    }

    if (track == 0) {
      preview.synthAMask = synthOccupancyMask(
          currentPageScene.synthABanks[bank].patterns[index], eventCount);
    } else if (track == 1) {
      preview.synthBMask = synthOccupancyMask(
          currentPageScene.synthBBanks[bank].patterns[index], eventCount);
    } else {
      preview.drumMask = drumOccupancyMask(
          currentPageScene.drumBanks[bank].patterns[index], eventCount);
    }
    preview.resolvedMask |= maskForTrackIndex(track);
  }

  const uint16_t scaled = static_cast<uint16_t>(eventCount * 4u);
  preview.energy = static_cast<uint8_t>(scaled > 255u ? 255u : scaled);
  return true;
}

inline bool sanitize(PhraseBank& bank) {
  bool changed = false;
  if (bank.version != kPersistenceVersion) {
    reset(bank);
    return true;
  }
  if (bank.nextPhraseId == kNoPhraseId) {
    bank.nextPhraseId = 1;
    changed = true;
  }

  for (int slot = 0; slot < kSlotCount; ++slot) {
    PhraseSlot& phrase = bank.slots[slot];
    if ((phrase.metadata.flags & kFlagValid) == 0) {
      PhraseSlot empty{};
      clearSlotValue(empty);
      if (std::memcmp(&phrase, &empty, sizeof(PhraseSlot)) != 0) {
        phrase = empty;
        changed = true;
      }
      continue;
    }

    if (!isValid(phrase) ||
        static_cast<int>(phrase.metadata.sourceStartRow) +
                phrase.metadata.lengthBars >
            Song::kMaxPositions) {
      clearSlotValue(phrase);
      changed = true;
      continue;
    }

    for (int bar = 0; bar < kMaxBars; ++bar) {
      for (int track = 0; track < kTrackCount; ++track) {
        int16_t& reference = phrase.patternRefs[bar][track];
        if (bar >= phrase.metadata.lengthBars ||
            (phrase.metadata.trackMask & maskForTrackIndex(track)) == 0) {
          if (reference != -1) {
            reference = -1;
            changed = true;
          }
          continue;
        }
        if (reference < -1 || reference >= kMaxGlobalPatterns) {
          reference = -1;
          changed = true;
        }
      }
    }
  }
  return changed;
}

}  // namespace PhraseCore
