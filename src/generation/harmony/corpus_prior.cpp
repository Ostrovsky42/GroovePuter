#include "corpus_prior.h"

namespace GroovePuterHarmony {
namespace {

bool validToken(const CanonicalFunctionToken& token) {
  return (token.degree < 7 || token.degree == kNoFunctionDegree) &&
         token.rootOffset >= -6 && token.rootOffset <= 5 &&
         token.qualityGroup <= 8 && (token.extensionFlags & 0xf000u) == 0 &&
         (token.slashBassClass < 12 || token.slashBassClass == kNoSlashBassClass);
}

bool lessToken(const CanonicalFunctionToken& a,
               const CanonicalFunctionToken& b) {
  if (a.degree != b.degree) return a.degree < b.degree;
  if (a.rootOffset != b.rootOffset) return a.rootOffset < b.rootOffset;
  if (a.qualityGroup != b.qualityGroup) return a.qualityGroup < b.qualityGroup;
  if (a.extensionFlags != b.extensionFlags)
    return a.extensionFlags < b.extensionFlags;
  if (a.alterationFlags != b.alterationFlags)
    return a.alterationFlags < b.alterationFlags;
  if (a.slashBassClass != b.slashBassClass)
    return a.slashBassClass < b.slashBassClass;
  return a.spellingFlags < b.spellingFlags;
}

bool equalToken(const CanonicalFunctionToken& a,
                const CanonicalFunctionToken& b) {
  return !lessToken(a, b) && !lessToken(b, a);
}

int32_t findToken(const CorpusModelView& model,
                  const CanonicalFunctionToken& token) {
  uint16_t first = 0;
  uint16_t count = model.tokenCount;
  while (count > 0) {
    const uint16_t step = static_cast<uint16_t>(count / 2);
    const uint16_t index = static_cast<uint16_t>(first + step);
    if (lessToken(model.tokens[index], token)) {
      first = static_cast<uint16_t>(index + 1);
      count = static_cast<uint16_t>(count - step - 1);
    } else {
      count = step;
    }
  }
  if (first < model.tokenCount && equalToken(model.tokens[first], token))
    return first;
  return -1;
}

uint16_t read16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint8_t lookupBigram(const uint8_t* bytes, uint16_t rows,
                     uint16_t previous, uint16_t target) {
  uint16_t low = 0;
  uint16_t high = rows;
  while (low < high) {
    const uint16_t middle = static_cast<uint16_t>(low + (high - low) / 2);
    const uint8_t* row = bytes + static_cast<uint32_t>(middle) * 5u;
    const uint16_t rowPrevious = read16(row);
    const uint16_t rowTarget = read16(row + 2);
    if (rowPrevious < previous || (rowPrevious == previous && rowTarget < target)) {
      low = static_cast<uint16_t>(middle + 1);
    } else {
      high = middle;
    }
  }
  if (low >= rows) return 0;
  const uint8_t* row = bytes + static_cast<uint32_t>(low) * 5u;
  return read16(row) == previous && read16(row + 2) == target ? row[4] : 0;
}

uint8_t lookupTrigram(const uint8_t* bytes, uint16_t rows,
                      uint16_t previous2, uint16_t previous1,
                      uint16_t target) {
  uint16_t low = 0;
  uint16_t high = rows;
  while (low < high) {
    const uint16_t middle = static_cast<uint16_t>(low + (high - low) / 2);
    const uint8_t* row = bytes + static_cast<uint32_t>(middle) * 7u;
    const uint16_t rowPrevious2 = read16(row);
    const uint16_t rowPrevious1 = read16(row + 2);
    const uint16_t rowTarget = read16(row + 4);
    if (rowPrevious2 < previous2 ||
        (rowPrevious2 == previous2 && rowPrevious1 < previous1) ||
        (rowPrevious2 == previous2 && rowPrevious1 == previous1 &&
         rowTarget < target)) {
      low = static_cast<uint16_t>(middle + 1);
    } else {
      high = middle;
    }
  }
  if (low >= rows) return 0;
  const uint8_t* row = bytes + static_cast<uint32_t>(low) * 7u;
  return read16(row) == previous2 && read16(row + 2) == previous1 &&
                 read16(row + 4) == target
             ? row[6]
             : 0;
}

uint8_t rootMotionClass(const CanonicalFunctionToken& from,
                        const CanonicalFunctionToken& to) {
  int16_t delta = static_cast<int16_t>(to.rootOffset) - from.rootOffset;
  while (delta < 0) delta += 12;
  while (delta >= 12) delta -= 12;
  return static_cast<uint8_t>(delta < 12 - delta ? delta : 12 - delta);
}

uint8_t lookupCoarse(const uint8_t* bytes, uint16_t rows,
                     uint8_t fromGroup, uint8_t motion, uint8_t toGroup) {
  uint16_t low = 0;
  uint16_t high = rows;
  while (low < high) {
    const uint16_t middle = static_cast<uint16_t>(low + (high - low) / 2);
    const uint8_t* row = bytes + static_cast<uint32_t>(middle) * 4u;
    if (row[0] < fromGroup ||
        (row[0] == fromGroup && row[1] < motion) ||
        (row[0] == fromGroup && row[1] == motion && row[2] < toGroup)) {
      low = static_cast<uint16_t>(middle + 1);
    } else {
      high = middle;
    }
  }
  if (low >= rows) return 0;
  const uint8_t* row = bytes + static_cast<uint32_t>(low) * 4u;
  return row[0] == fromGroup && row[1] == motion && row[2] == toGroup
             ? row[3]
             : 0;
}

}  // namespace

uint8_t corpusPrior(const CanonicalFunctionToken& previous2,
                    const CanonicalFunctionToken& previous1,
                    const CanonicalFunctionToken& target,
                    const CorpusModelView& model) {
  if (!validToken(previous2) || !validToken(previous1) || !validToken(target))
    return 0;
  const int32_t previous2Id = findToken(model, previous2);
  const int32_t previous1Id = findToken(model, previous1);
  const int32_t targetId = findToken(model, target);
  if (previous2Id >= 0 && previous1Id >= 0 && targetId >= 0 &&
      model.trigramBytes != nullptr) {
    const uint8_t prior = lookupTrigram(
        model.trigramBytes, model.trigramCount,
        static_cast<uint16_t>(previous2Id), static_cast<uint16_t>(previous1Id),
        static_cast<uint16_t>(targetId));
    if (prior > 0) return prior;
  }
  if (previous1Id >= 0 && targetId >= 0 && model.bigramBytes != nullptr) {
    const uint8_t prior = lookupBigram(
        model.bigramBytes, model.bigramCount,
        static_cast<uint16_t>(previous1Id), static_cast<uint16_t>(targetId));
    if (prior > 0) return prior;
  }
  if (model.coarseBytes != nullptr) {
    return lookupCoarse(model.coarseBytes, model.coarseCount,
                        previous1.qualityGroup,
                        rootMotionClass(previous1, target), target.qualityGroup);
  }
  return 0;
}

}  // namespace GroovePuterHarmony
