#include "scenes.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <memory>

namespace {
int clampIndex(int value, int maxExclusive) {
  if (value < 0) return 0;
  if (value >= maxExclusive) return maxExclusive - 1;
  return value;
}

int clampAutomationValue(int value, int maxValue) {
  if (value < 0) return 0;
  if (value > maxValue) return maxValue;
  return value;
}

struct AutomationBlock {
  uint16_t start;
  uint16_t length;
};

// Shared pool for automation nodes to keep per-pattern memory small.
class AutomationNodePool {
 public:
  AutomationNodePool() {
    freeBlocks_[0] = {0, static_cast<uint16_t>(kAutomationPoolNodes)};
    freeCount_ = 1;
  }

  AutomationNode* data() { return nodes_; }
  const AutomationNode* data() const { return nodes_; }

  bool ensureCapacity(AutomationLane& lane, int needed) {
    if (needed <= 0) return true;
    uint16_t need = static_cast<uint16_t>(needed);
    if (lane.capacity >= need) return true;

    uint16_t newCap = need;
    uint16_t grow = lane.capacity > 0 ? static_cast<uint16_t>(lane.capacity * 2) : static_cast<uint16_t>(4);
    if (grow > newCap) newCap = grow;
    if (newCap > kAutomationMaxNodes) newCap = static_cast<uint16_t>(kAutomationMaxNodes);
    if (newCap < need) return false;

    uint16_t newStart = kAutomationInvalidIndex;
    if (!reserveBlock(newCap, newStart)) {
      if (newCap == need || !reserveBlock(need, newStart)) {
        return false;
      }
      newCap = need;
    }

    if (lane.nodeCount > 0 && lane.start != kAutomationInvalidIndex) {
      std::memmove(nodes_ + newStart, nodes_ + lane.start,
                   static_cast<size_t>(lane.nodeCount) * sizeof(AutomationNode));
      freeBlock(lane.start, lane.capacity);
    } else if (lane.start != kAutomationInvalidIndex && lane.capacity > 0) {
      freeBlock(lane.start, lane.capacity);
    }
    lane.start = newStart;
    lane.capacity = newCap;
    return true;
  }

  void release(AutomationLane& lane) {
    if (lane.start != kAutomationInvalidIndex && lane.capacity > 0) {
      freeBlock(lane.start, lane.capacity);
    }
    lane.start = kAutomationInvalidIndex;
    lane.capacity = 0;
  }

 private:
  bool reserveBlock(uint16_t length, uint16_t& outStart) {
    for (int i = 0; i < freeCount_; ++i) {
      if (freeBlocks_[i].length >= length) {
        outStart = freeBlocks_[i].start;
        freeBlocks_[i].start = static_cast<uint16_t>(freeBlocks_[i].start + length);
        freeBlocks_[i].length = static_cast<uint16_t>(freeBlocks_[i].length - length);
        if (freeBlocks_[i].length == 0) {
          removeBlock(i);
        }
        return true;
      }
    }
    return false;
  }

  void freeBlock(uint16_t start, uint16_t length) {
    if (length == 0 || freeCount_ >= kMaxBlocks) return;
    insertBlock(start, length);
  }

  void insertBlock(uint16_t start, uint16_t length) {
    int pos = 0;
    while (pos < freeCount_ && freeBlocks_[pos].start < start) {
      ++pos;
    }
    if (freeCount_ >= kMaxBlocks) return;
    for (int i = freeCount_; i > pos; --i) {
      freeBlocks_[i] = freeBlocks_[i - 1];
    }
    freeBlocks_[pos] = {start, length};
    freeCount_++;
    mergeAdjacent();
  }

  void mergeAdjacent() {
    if (freeCount_ < 2) return;
    int write = 0;
    for (int i = 1; i < freeCount_; ++i) {
      AutomationBlock& cur = freeBlocks_[write];
      AutomationBlock next = freeBlocks_[i];
      if (static_cast<uint32_t>(cur.start) + cur.length == next.start) {
        cur.length = static_cast<uint16_t>(cur.length + next.length);
      } else {
        ++write;
        freeBlocks_[write] = next;
      }
    }
    freeCount_ = write + 1;
  }

  void removeBlock(int index) {
    for (int i = index; i + 1 < freeCount_; ++i) {
      freeBlocks_[i] = freeBlocks_[i + 1];
    }
    freeCount_--;
  }

  static constexpr int kMaxBlocks = 64;
  AutomationNode nodes_[kAutomationPoolNodes]{};
  AutomationBlock freeBlocks_[kMaxBlocks]{};
  int freeCount_ = 0;
};

AutomationNodePool& automationPool() {
  static AutomationNodePool pool;
  return pool;
}

bool appendAutomationNode(AutomationLane& lane, uint8_t x, uint8_t y) {
  if (lane.nodeCount >= kAutomationMaxNodes) return false;
  if (!lane.ensureCapacity(lane.nodeCount + 1)) return false;
  AutomationNode* nodes = lane.nodes();
  if (!nodes) return false;
  if (lane.nodeCount > 0) {
    uint8_t lastX = nodes[lane.nodeCount - 1].x;
    if (x < lastX) return false;
    if (x == lastX) {
      int sameCount = 1;
      for (int i = lane.nodeCount - 2; i >= 0; --i) {
        if (nodes[i].x != lastX) break;
        ++sameCount;
      }
      if (sameCount >= 2) return false;
    }
  }
  nodes[lane.nodeCount++] = AutomationNode{x, y};
  return true;
}

void clearDrumPattern(DrumPattern& pattern) {
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    pattern.steps[i].hit = false;
  }
}

void clearDrumPatternSet(DrumPatternSet& patternSet) {
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    clearDrumPattern(patternSet.voices[v]);
  }
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    patternSet.accents[i] = false;
  }
  for (int i = 0; i < static_cast<int>(DrumAutomationParamId::Count); ++i) {
    patternSet.automation[i].clear();
  }
}

void clearSynthPattern(SynthPattern& pattern) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
    pattern.steps[i].slide = false;
    pattern.steps[i].accent = false;
  }
  for (int i = 0; i < static_cast<int>(TB303ParamId::Count); ++i) {
    pattern.automation[i].clear();
  }
}

void clearSong(Song& song) {
  song.length = 1;
  for (int i = 0; i < Song::kMaxPositions; ++i) {
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      song.positions[i].patterns[t] = -1;
    }
  }
}

void clearSceneData(Scene& scene) {
  for (int b = 0; b < kBankCount; ++b) {
    for (int p = 0; p < Bank<DrumPatternSet>::kPatterns; ++p) {
      clearDrumPatternSet(scene.drumBanks[b].patterns[p]);
    }
    for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
      clearSynthPattern(scene.synthABanks[b].patterns[p]);
      clearSynthPattern(scene.synthBBanks[b].patterns[p]);
    }
  }
  clearSong(scene.song);
}
}

bool AutomationLane::hasNodes() const { return nodeCount > 0; }

bool AutomationLane::hasOptions() const { return optionCount > 0; }

AutomationLane::AutomationLane(const AutomationLane& other)
    : start(other.start),
      capacity(other.capacity),
      nodeCount(other.nodeCount),
      enabled(other.enabled),
      optionCount(other.optionCount),
      optionLabels(nullptr) {
  if (!other.optionLabels) return;
  constexpr size_t kLabelBytes =
      static_cast<size_t>(kAutomationMaxOptions) * kAutomationOptionLabelMaxLen;
  optionLabels = static_cast<char*>(std::malloc(kLabelBytes));
  if (!optionLabels) return;
  std::memcpy(optionLabels, other.optionLabels, kLabelBytes);
}

AutomationLane& AutomationLane::operator=(const AutomationLane& other) {
  if (this == &other) return *this;
  start = other.start;
  capacity = other.capacity;
  nodeCount = other.nodeCount;
  enabled = other.enabled;
  optionCount = other.optionCount;
  if (optionLabels) {
    std::free(optionLabels);
    optionLabels = nullptr;
  }
  if (!other.optionLabels) return *this;
  constexpr size_t kLabelBytes =
      static_cast<size_t>(kAutomationMaxOptions) * kAutomationOptionLabelMaxLen;
  optionLabels = static_cast<char*>(std::malloc(kLabelBytes));
  if (!optionLabels) return *this;
  std::memcpy(optionLabels, other.optionLabels, kLabelBytes);
  return *this;
}

AutomationLane::~AutomationLane() { clearOptions(); }

void AutomationLane::clear() {
  automationPool().release(*this);
  nodeCount = 0;
  enabled = false;
  clearOptions();
}

uint8_t AutomationLane::evaluate(float t) const {
  if (nodeCount == 0) return 0;
  const AutomationNode* laneNodes = nodes();
  if (!laneNodes) return 0;
  auto clampOption = [&](int value) -> uint8_t {
    if (optionCount <= 1) return 0;
    int maxValue = static_cast<int>(optionCount) - 1;
    if (value < 0) value = 0;
    if (value > maxValue) value = maxValue;
    return static_cast<uint8_t>(value);
  };

  if (hasOptions()) {
    if (nodeCount == 1) return clampOption(laneNodes[0].y);
    if (t <= static_cast<float>(laneNodes[0].x)) return clampOption(laneNodes[0].y);
    for (int i = 1; i < nodeCount; ++i) {
      float x1 = static_cast<float>(laneNodes[i].x);
      if (t < x1) {
        const AutomationNode& n0 = laneNodes[i - 1];
        return clampOption(n0.y);
      }
    }
    return clampOption(laneNodes[nodeCount - 1].y);
  }

  if (nodeCount == 1) return laneNodes[0].y;
  if (t <= static_cast<float>(laneNodes[0].x)) return laneNodes[0].y;
  for (int i = 1; i < nodeCount; ++i) {
    float x1 = static_cast<float>(laneNodes[i].x);
    if (t < x1) {
      const AutomationNode& n0 = laneNodes[i - 1];
      const AutomationNode& n1 = laneNodes[i];
      if (n0.x == n1.x) return n0.y;
      float x0 = static_cast<float>(n0.x);
      float alpha = (t - x0) / (x1 - x0);
      float y = static_cast<float>(n0.y) + alpha * (static_cast<float>(n1.y) - static_cast<float>(n0.y));
      if (y < 0.0f) y = 0.0f;
      if (y > 255.0f) y = 255.0f;
      return static_cast<uint8_t>(y + 0.5f);
    }
  }
  return laneNodes[nodeCount - 1].y;
}

AutomationNode* AutomationLane::nodes() {
  if (start == kAutomationInvalidIndex || capacity == 0) return nullptr;
  return automationPool().data() + start;
}

const AutomationNode* AutomationLane::nodes() const {
  if (start == kAutomationInvalidIndex || capacity == 0) return nullptr;
  return automationPool().data() + start;
}

bool AutomationLane::ensureCapacity(int needed) {
  return automationPool().ensureCapacity(*this, needed);
}

void AutomationLane::clearOptions() {
  optionCount = 0;
  if (optionLabels) {
    std::free(optionLabels);
    optionLabels = nullptr;
  }
}

void AutomationLane::setOptions(const char* const* labels, int count) {
  clearOptions();
  if (!labels || count <= 0) return;
  int limit = count;
  if (limit > kAutomationMaxOptions) limit = kAutomationMaxOptions;
  optionLabels = static_cast<char*>(std::calloc(
      static_cast<size_t>(kAutomationMaxOptions) * kAutomationOptionLabelMaxLen,
      sizeof(char)));
  if (!optionLabels) return;
  optionCount = static_cast<uint8_t>(limit);
  for (int i = 0; i < limit; ++i) {
    if (!labels[i]) continue;
    setOptionLabel(i, labels[i]);
  }
}

void AutomationLane::setOptionLabel(int index, const char* label) {
  if (index < 0 || index >= kAutomationMaxOptions) return;
  if (!optionLabels) {
    optionLabels = static_cast<char*>(std::calloc(
        static_cast<size_t>(kAutomationMaxOptions) * kAutomationOptionLabelMaxLen,
        sizeof(char)));
    if (!optionLabels) return;
  }
  if (!label) label = "";
  char* dst = optionLabels + index * kAutomationOptionLabelMaxLen;
  std::strncpy(dst, label, kAutomationOptionLabelMaxLen - 1);
  dst[kAutomationOptionLabelMaxLen - 1] = '\0';
  if (optionCount < static_cast<uint8_t>(index + 1)) {
    optionCount = static_cast<uint8_t>(index + 1);
  }
}

const char* AutomationLane::optionLabelAt(int index) const {
  if (index < 0 || index >= optionCount) return nullptr;
  if (!optionLabels) return nullptr;
  const char* label = optionLabels + index * kAutomationOptionLabelMaxLen;
  if (!label[0]) return nullptr;
  return label;
}

int AutomationLane::clampOptionIndex(int index) const {
  if (optionCount <= 1) return 0;
  int maxValue = static_cast<int>(optionCount) - 1;
  if (index < 0) return 0;
  if (index > maxValue) return maxValue;
  return index;
}

bool SynthPattern::hasAutomationLane(TB303ParamId id) const {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(TB303ParamId::Count)) return false;
  return automation[idx].enabled && automation[idx].nodeCount > 0;
}

bool DrumPatternSet::hasAutomationLane(DrumAutomationParamId id) const {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(DrumAutomationParamId::Count)) return false;
  return automation[idx].enabled && automation[idx].nodeCount > 0;
}

const AutomationLane* SynthPattern::automationLane(TB303ParamId id) const {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(TB303ParamId::Count)) return nullptr;
  return &automation[idx];
}

const AutomationLane* DrumPatternSet::automationLane(DrumAutomationParamId id) const {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(DrumAutomationParamId::Count)) return nullptr;
  return &automation[idx];
}

AutomationLane* SynthPattern::editAutomationLane(TB303ParamId id) {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(TB303ParamId::Count)) return nullptr;
  automation[idx].enabled = true;
  return &automation[idx];
}

AutomationLane* DrumPatternSet::editAutomationLane(DrumAutomationParamId id) {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(DrumAutomationParamId::Count)) return nullptr;
  automation[idx].enabled = true;
  return &automation[idx];
}

void SynthPattern::clearAutomationLane(TB303ParamId id) {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(TB303ParamId::Count)) return;
  automation[idx].clear();
}

void DrumPatternSet::clearAutomationLane(DrumAutomationParamId id) {
  int idx = static_cast<int>(id);
  if (idx < 0 || idx >= static_cast<int>(DrumAutomationParamId::Count)) return;
  automation[idx].clear();
}

SceneJsonObserver::SceneJsonObserver(Scene& scene, float defaultBpm)
    : target_(scene), bpm_(defaultBpm) {
  clearSong(song_);
}

SceneJsonObserver::Path SceneJsonObserver::deduceArrayPath(const Context& parent) const {
  switch (parent.path) {
  case Path::DrumBanks:
    return Path::DrumBank;
  case Path::DrumBank:
    return Path::DrumPatternSet;
  case Path::SynthABanks:
    return Path::SynthABank;
  case Path::SynthABank:
    return Path::SynthSteps;
  case Path::SynthBBanks:
    return Path::SynthBBank;
  case Path::SynthBBank:
    return Path::SynthSteps;
  case Path::SynthAutomation:
    return Path::SynthAutomationLane;
  case Path::DrumAutomation:
    return Path::DrumAutomationLane;
  case Path::SynthParams:
    return Path::SynthParam;
  case Path::SynthDistortion:
    return Path::SynthDistortion;
  case Path::SynthDelay:
    return Path::SynthDelay;
  case Path::Song:
    return Path::SongPosition;
  default:
    return Path::Unknown;
  }
}

SceneJsonObserver::Path SceneJsonObserver::deduceObjectPath(const Context& parent) const {
  switch (parent.path) {
  case Path::DrumBank:
    return Path::DrumPatternSet;
  case Path::DrumPatternSet:
    return Path::DrumVoice;
  case Path::DrumAutomation:
    return Path::DrumAutomationLane;
  case Path::SynthABank:
  case Path::SynthBBank:
    return Path::SynthPattern;
  case Path::SynthSteps:
    return Path::SynthStep;
  case Path::SynthAutomation:
    return Path::SynthAutomationLane;
  case Path::SynthAutomationNodes:
    return Path::SynthAutomationNode;
  case Path::DrumAutomationNodes:
    return Path::DrumAutomationNode;
  case Path::SynthParams:
    return Path::SynthParam;
  case Path::SongPositions:
    return Path::SongPosition;
  default:
    return Path::Unknown;
  }
}

int SceneJsonObserver::currentIndexFor(Path path) const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == path && stack_[i].type == Context::Type::Array) {
      return stack_[i].index;
    }
  }
  return -1;
}

bool SceneJsonObserver::inSynthBankA() const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == Path::SynthABanks || stack_[i].path == Path::SynthABank) return true;
    if (stack_[i].path == Path::SynthBBanks || stack_[i].path == Path::SynthBBank) return false;
  }
  return true;
}

bool SceneJsonObserver::inSynthBankB() const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == Path::SynthBBanks || stack_[i].path == Path::SynthBBank) return true;
    if (stack_[i].path == Path::SynthABanks || stack_[i].path == Path::SynthABank) return false;
  }
  return false;
}

const char* SceneJsonObserver::pathName(Path path) {
  switch (path) {
    case Path::Root: return "Root";
    case Path::DrumBanks: return "DrumBanks";
    case Path::DrumBank: return "DrumBank";
    case Path::DrumPatternSet: return "DrumPatternSet";
    case Path::DrumVoice: return "DrumVoice";
    case Path::DrumHitArray: return "DrumHitArray";
    case Path::DrumAccentArray: return "DrumAccentArray";
    case Path::DrumAutomation: return "DrumAutomation";
    case Path::DrumAutomationLane: return "DrumAutomationLane";
    case Path::DrumAutomationNodes: return "DrumAutomationNodes";
    case Path::DrumAutomationOptions: return "DrumAutomationOptions";
    case Path::DrumAutomationNode: return "DrumAutomationNode";
    case Path::SynthABanks: return "SynthABanks";
    case Path::SynthABank: return "SynthABank";
    case Path::SynthBBanks: return "SynthBBanks";
    case Path::SynthBBank: return "SynthBBank";
    case Path::SynthPattern: return "SynthPattern";
    case Path::SynthSteps: return "SynthSteps";
    case Path::SynthStep: return "SynthStep";
    case Path::SynthAutomation: return "SynthAutomation";
    case Path::SynthAutomationLane: return "SynthAutomationLane";
    case Path::SynthAutomationNodes: return "SynthAutomationNodes";
    case Path::SynthAutomationOptions: return "SynthAutomationOptions";
    case Path::SynthAutomationNode: return "SynthAutomationNode";
    case Path::State: return "State";
    case Path::SynthPatternIndex: return "SynthPatternIndex";
    case Path::SynthBankIndex: return "SynthBankIndex";
    case Path::Mute: return "Mute";
    case Path::MuteDrums: return "MuteDrums";
    case Path::MuteSynth: return "MuteSynth";
    case Path::SynthDistortion: return "SynthDistortion";
    case Path::SynthDelay: return "SynthDelay";
    case Path::SynthParams: return "SynthParams";
    case Path::SynthParam: return "SynthParam";
    case Path::Song: return "Song";
    case Path::SongPositions: return "SongPositions";
    case Path::SongPosition: return "SongPosition";
    case Path::Unknown: return "Unknown";
  }
  return "Unknown";
}

void SceneJsonObserver::pushContext(Context::Type type, Path path) {
  if (stackSize_ >= kMaxStack) {
    SCENE_DEBUG_PRINTF("SceneJsonObserver: stack overflow at %s\n", pathName(path));
    error_ = true;
    return;
  }
  stack_[stackSize_++] = Context{type, path, 0};
}

void SceneJsonObserver::popContext() {
  if (stackSize_ == 0) {
    SCENE_DEBUG_PRINTLN("SceneJsonObserver: popContext on empty stack");
    error_ = true;
    return;
  }
  --stackSize_;
}

void SceneJsonObserver::onObjectStart() {
  if (error_) return;
  Path path = Path::Unknown;
  Path parentPath = Path::Unknown;
  Context::Type parentType = Context::Type::Object;
  if (stackSize_ == 0) {
    path = Path::Root;
  } else {
    const Context& parent = stack_[stackSize_ - 1];
    parentPath = parent.path;
    parentType = parent.type;
    if (parent.type == Context::Type::Array) {
      path = deduceObjectPath(parent);
    } else if (parent.path == Path::Root && lastKey_ == "state") {
      path = Path::State;
    } else if (parent.path == Path::Root && lastKey_ == "song") {
      path = Path::Song;
    } else if (parent.path == Path::Root && lastKey_ == "drumBanks") {
      path = Path::DrumBanks;
    } else if (parent.path == Path::Root && lastKey_ == "synthABanks") {
      path = Path::SynthABanks;
    } else if (parent.path == Path::Root && lastKey_ == "synthBBanks") {
      path = Path::SynthBBanks;
    } else if (parent.path == Path::State && lastKey_ == "mute") {
      path = Path::Mute;
    }
  }
  pushContext(Context::Type::Object, path);
  if (path == Path::Unknown) {
    SCENE_DEBUG_PRINTF(
        "SceneJsonObserver: unknown object path key='%s' parent=%s type=%s\n",
        lastKey_.c_str(),
        pathName(parentPath),
        parentType == Context::Type::Array ? "array" : "object");
    error_ = true;
  }
  if (path == Path::SynthAutomationLane) {
    automationParam_ = -1;
    automationEnabled_ = true;
  } else if (path == Path::DrumAutomationLane) {
    drumAutomationParam_ = -1;
    drumAutomationEnabled_ = true;
  } else if (path == Path::SynthAutomationNode) {
    automationNodeHasX_ = false;
    automationNodeHasY_ = false;
  } else if (path == Path::DrumAutomationNode) {
    drumAutomationNodeHasX_ = false;
    drumAutomationNodeHasY_ = false;
  }
}

void SceneJsonObserver::onObjectEnd() {
  if (error_) return;
  if (stackSize_ > 0) {
    Path path = stack_[stackSize_ - 1].path;
    if (path == Path::SynthAutomationNode) {
      if (automationParam_ >= 0 && automationParam_ < static_cast<int>(TB303ParamId::Count) &&
          automationNodeHasX_ && automationNodeHasY_) {
        bool useBankB = inSynthBankB();
        int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
        if (bankIdx < 0) bankIdx = 0;
        int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
        if (patternIdx >= 0 && patternIdx < Bank<SynthPattern>::kPatterns &&
            bankIdx >= 0 && bankIdx < kBankCount) {
          SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                           : target_.synthABanks[bankIdx].patterns[patternIdx];
          int x = clampAutomationValue(automationNodeX_, kAutomationMaxX);
          AutomationLane& lane = pattern.automation[automationParam_];
          int maxY = lane.optionCount > 0 ? lane.optionCount - 1 : 255;
          int y = clampAutomationValue(automationNodeY_, maxY);
          appendAutomationNode(lane, static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        }
      }
    } else if (path == Path::DrumAutomationNode) {
      if (drumAutomationParam_ >= 0 &&
          drumAutomationParam_ < static_cast<int>(DrumAutomationParamId::Count) &&
          drumAutomationNodeHasX_ && drumAutomationNodeHasY_) {
        int bankIdx = currentIndexFor(Path::DrumBanks);
        if (bankIdx < 0) bankIdx = 0;
        int patternIdx = currentIndexFor(Path::DrumBank);
        if (patternIdx >= 0 && patternIdx < Bank<DrumPatternSet>::kPatterns &&
            bankIdx >= 0 && bankIdx < kBankCount) {
          DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
          int x = clampAutomationValue(drumAutomationNodeX_, kAutomationMaxX);
          AutomationLane& lane = patternSet.automation[drumAutomationParam_];
          int maxY = lane.optionCount > 0 ? lane.optionCount - 1 : 255;
          int y = clampAutomationValue(drumAutomationNodeY_, maxY);
          appendAutomationNode(lane, static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        }
      }
    } else if (path == Path::SynthAutomationLane) {
      automationParam_ = -1;
      automationEnabled_ = true;
    } else if (path == Path::DrumAutomationLane) {
      drumAutomationParam_ = -1;
      drumAutomationEnabled_ = true;
    }
  }
  popContext();
}

void SceneJsonObserver::onArrayStart() {
  if (error_) return;
  Path path = Path::Unknown;
  if (stackSize_ > 0) {
    const Context& parent = stack_[stackSize_ - 1];
    if (parent.type == Context::Type::Object) {
      if (parent.path == Path::Root) {
        if (lastKey_ == "drumBanks") path = Path::DrumBanks;
        else if (lastKey_ == "drumBank") path = Path::DrumBank;
        else if (lastKey_ == "synthABanks") path = Path::SynthABanks;
        else if (lastKey_ == "synthABank") path = Path::SynthABank;
        else if (lastKey_ == "synthBBanks") path = Path::SynthBBanks;
        else if (lastKey_ == "synthBBank") path = Path::SynthBBank;
      } else if (parent.path == Path::DrumBanks) {
        if (lastKey_ == "banks" || lastKey_ == "patterns") path = Path::DrumBanks;
      } else if (parent.path == Path::Song) {
        if (lastKey_ == "positions") path = Path::SongPositions;
        else if (lastKey_ == "synthDistortion") path = Path::SynthDistortion;
        else if (lastKey_ == "synthDelay") path = Path::SynthDelay;
      } else if (parent.path == Path::DrumVoice) {
        if (lastKey_ == "hit") path = Path::DrumHitArray;
        else if (lastKey_ == "accent") path = Path::DrumAccentArray;
      } else if (parent.path == Path::DrumPatternSet) {
        if (lastKey_ == "voices") path = Path::DrumPatternSet;
        else if (lastKey_ == "accent") path = Path::DrumAccentArray;
        else if (lastKey_ == "automation") path = Path::DrumAutomation;
      } else if (parent.path == Path::State) {
        if (lastKey_ == "synthPatternIndex") path = Path::SynthPatternIndex;
        else if (lastKey_ == "synthBankIndex") path = Path::SynthBankIndex;
        else if (lastKey_ == "synthDistortion") path = Path::SynthDistortion;
        else if (lastKey_ == "synthDelay") path = Path::SynthDelay;
        else if (lastKey_ == "synthParams") path = Path::SynthParams;
      } else if (parent.path == Path::SynthPattern) {
        if (lastKey_ == "steps") path = Path::SynthSteps;
        else if (lastKey_ == "automation") path = Path::SynthAutomation;
      } else if (parent.path == Path::SynthAutomationLane) {
        if (lastKey_ == "nodes") path = Path::SynthAutomationNodes;
        else if (lastKey_ == "options") path = Path::SynthAutomationOptions;
      } else if (parent.path == Path::DrumAutomationLane) {
        if (lastKey_ == "nodes") path = Path::DrumAutomationNodes;
        else if (lastKey_ == "options") path = Path::DrumAutomationOptions;
      } else if (parent.path == Path::Mute) {
        if (lastKey_ == "drums") path = Path::MuteDrums;
        else if (lastKey_ == "synth") path = Path::MuteSynth;
      }
    } else if (parent.type == Context::Type::Array) {
      path = deduceArrayPath(parent);
    }
  }
  pushContext(Context::Type::Array, path);
  if (path == Path::Unknown) {
    SCENE_DEBUG_PRINTF("SceneJsonObserver: unknown array path for key '%s'\n", lastKey_.c_str());
    error_ = true;
  }
}

void SceneJsonObserver::onArrayEnd() {
  if (error_) return;
  if (stackSize_ > 0) {
    Path path = stack_[stackSize_ - 1].path;
    if (path == Path::SynthAutomationOptions) {
      if (automationParam_ >= 0 && automationParam_ < static_cast<int>(TB303ParamId::Count)) {
        bool useBankB = inSynthBankB();
        int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
        if (bankIdx < 0) bankIdx = 0;
        int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
        if (patternIdx >= 0 && patternIdx < Bank<SynthPattern>::kPatterns &&
            bankIdx >= 0 && bankIdx < kBankCount) {
          SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                           : target_.synthABanks[bankIdx].patterns[patternIdx];
          AutomationLane& lane = pattern.automation[automationParam_];
          int maxY = lane.optionCount > 0 ? lane.optionCount - 1 : 0;
          AutomationNode* nodes = lane.nodes();
          if (nodes) {
            for (int i = 0; i < lane.nodeCount; ++i) {
              if (nodes[i].y > maxY) nodes[i].y = static_cast<uint8_t>(maxY);
            }
          }
        }
      }
    } else if (path == Path::DrumAutomationOptions) {
      if (drumAutomationParam_ >= 0 &&
          drumAutomationParam_ < static_cast<int>(DrumAutomationParamId::Count)) {
        int bankIdx = currentIndexFor(Path::DrumBanks);
        if (bankIdx < 0) bankIdx = 0;
        int patternIdx = currentIndexFor(Path::DrumBank);
        if (patternIdx >= 0 && patternIdx < Bank<DrumPatternSet>::kPatterns &&
            bankIdx >= 0 && bankIdx < kBankCount) {
          DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
          AutomationLane& lane = patternSet.automation[drumAutomationParam_];
          int maxY = lane.optionCount > 0 ? lane.optionCount - 1 : 0;
          AutomationNode* nodes = lane.nodes();
          if (nodes) {
            for (int i = 0; i < lane.nodeCount; ++i) {
              if (nodes[i].y > maxY) nodes[i].y = static_cast<uint8_t>(maxY);
            }
          }
        }
      }
    }
  }
  popContext();
}

void SceneJsonObserver::handlePrimitiveNumber(double value, bool isInteger) {
  (void)isInteger;
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::Song) {
    if (lastKey_ == "length") {
      int len = static_cast<int>(value);
      if (len < 1) len = 1;
      if (len > Song::kMaxPositions) len = Song::kMaxPositions;
      song_.length = len;
      hasSong_ = true;
    }
    return;
  }
  if (path == Path::SongPosition) {
    int posIdx = currentIndexFor(Path::SongPositions);
    if (posIdx < 0 || posIdx >= Song::kMaxPositions) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid song position index %d\n", posIdx);
      error_ = true;
      return;
    }
    int trackIdx = -1;
    if (lastKey_ == "a") trackIdx = 0;
    else if (lastKey_ == "b") trackIdx = 1;
    else if (lastKey_ == "drums") trackIdx = 2;
    if (trackIdx >= 0 && trackIdx < SongPosition::kTrackCount) {
      song_.positions[posIdx].patterns[trackIdx] = clampSongPatternIndex(static_cast<int>(value));
      if (posIdx + 1 > song_.length) song_.length = posIdx + 1;
      hasSong_ = true;
    }
    return;
  }
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray ||
      path == Path::MuteDrums || path == Path::MuteSynth || path == Path::SynthDistortion ||
      path == Path::SynthDelay) {
    handlePrimitiveBool(value != 0);
    return;
  }
  if (path == Path::SynthPatternIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthPatternIndex_[idx] = static_cast<int>(value);
    return;
  }
  if (path == Path::SynthBankIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthBankIndex_[idx] = static_cast<int>(value);
    return;
  }
  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthSteps);
    bool useBankB = inSynthBankB();
    int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (stepIdx < 0 || stepIdx >= SynthPattern::kSteps ||
        patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth step idx=%d pattern=%d bank=%d\n",
                         stepIdx, patternIdx, bankIdx);
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                     : target_.synthABanks[bankIdx].patterns[patternIdx];
    if (lastKey_ == "note") {
      pattern.steps[stepIdx].note = static_cast<int>(value);
    } else if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value != 0;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value != 0;
    }
    return;
  }
  if (path == Path::SynthAutomationLane) {
    if (lastKey_ == "param") {
      int param = static_cast<int>(value);
      if (param < 0 || param >= static_cast<int>(TB303ParamId::Count)) {
        automationParam_ = -1;
        return;
      }
      automationParam_ = param;
      bool useBankB = inSynthBankB();
      int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
      if (bankIdx < 0) bankIdx = 0;
      int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
      if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
          bankIdx < 0 || bankIdx >= kBankCount) {
        automationParam_ = -1;
        return;
      }
      SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                       : target_.synthABanks[bankIdx].patterns[patternIdx];
      pattern.automation[automationParam_].clear();
      pattern.automation[automationParam_].enabled = automationEnabled_;
    }
    return;
  }
  if (path == Path::DrumAutomationLane) {
    if (lastKey_ == "param") {
      int param = static_cast<int>(value);
      if (param < 0 || param >= static_cast<int>(DrumAutomationParamId::Count)) {
        drumAutomationParam_ = -1;
        return;
      }
      drumAutomationParam_ = param;
      int bankIdx = currentIndexFor(Path::DrumBanks);
      if (bankIdx < 0) bankIdx = 0;
      int patternIdx = currentIndexFor(Path::DrumBank);
      if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
          bankIdx < 0 || bankIdx >= kBankCount) {
        drumAutomationParam_ = -1;
        return;
      }
      DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
      patternSet.automation[drumAutomationParam_].clear();
      patternSet.automation[drumAutomationParam_].enabled = drumAutomationEnabled_;
    }
    return;
  }
  if (path == Path::SynthAutomationNode) {
    if (lastKey_ == "x") {
      automationNodeHasX_ = true;
      automationNodeX_ = static_cast<int>(value);
    } else if (lastKey_ == "y") {
      automationNodeHasY_ = true;
      automationNodeY_ = static_cast<int>(value);
    }
    return;
  }
  if (path == Path::DrumAutomationNode) {
    if (lastKey_ == "x") {
      drumAutomationNodeHasX_ = true;
      drumAutomationNodeX_ = static_cast<int>(value);
    } else if (lastKey_ == "y") {
      drumAutomationNodeHasY_ = true;
      drumAutomationNodeY_ = static_cast<int>(value);
    }
    return;
  }
  if (path == Path::SynthParam) {
    int synthIdx = currentIndexFor(Path::SynthParams);
    if (synthIdx < 0 || synthIdx >= 2) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth param index %d\n", synthIdx);
      error_ = true;
      return;
    }
    float fval = static_cast<float>(value);
    if (lastKey_ == "cutoff") {
      synthParameters_[synthIdx].cutoff = fval;
    } else if (lastKey_ == "resonance") {
      synthParameters_[synthIdx].resonance = fval;
    } else if (lastKey_ == "envAmount") {
      synthParameters_[synthIdx].envAmount = fval;
    } else if (lastKey_ == "envDecay") {
      synthParameters_[synthIdx].envDecay = fval;
    } else if (lastKey_ == "oscType") {
      synthParameters_[synthIdx].oscType = static_cast<int>(value);
    }
    return;
  }
  if (path == Path::State) {
    if (lastKey_ == "bpm") {
      bpm_ = static_cast<float>(value);
      return;
    }
    if (lastKey_ == "songPosition") {
      songPosition_ = static_cast<int>(value);
      return;
    }
    if (lastKey_ == "songMode") {
      songMode_ = value != 0;
      return;
    }
    if (lastKey_ == "loopStart") {
      loopStartRow_ = static_cast<int>(value);
      return;
    }
    if (lastKey_ == "loopEnd") {
      loopEndRow_ = static_cast<int>(value);
      return;
    }
    int intValue = static_cast<int>(value);
    if (lastKey_ == "drumPatternIndex") {
      drumPatternIndex_ = intValue;
    } else if (lastKey_ == "drumBankIndex") {
      drumBankIndex_ = intValue;
    } else if (lastKey_ == "synthPatternIndex") {
      synthPatternIndex_[0] = intValue;
    } else if (lastKey_ == "synthBankIndex") {
      synthBankIndex_[0] = intValue;
    }
  }
}

void SceneJsonObserver::handlePrimitiveBool(bool value) {
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray) {
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
        stepIdx < 0 || stepIdx >= DrumPattern::kSteps ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid drum step idx=%d pattern=%d bank=%d\n",
                         stepIdx, patternIdx, bankIdx);
      error_ = true;
      return;
    }
    DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
    if (path == Path::DrumHitArray) {
      int voiceIdx = currentIndexFor(Path::DrumPatternSet);
      if (voiceIdx < 0 || voiceIdx >= DrumPatternSet::kVoices) {
        SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid drum voice index %d\n", voiceIdx);
        error_ = true;
        return;
      }
      patternSet.voices[voiceIdx].steps[stepIdx].hit = value;
    } else {
      int voiceIdx = currentIndexFor(Path::DrumPatternSet);
      if (voiceIdx >= 0 && voiceIdx < DrumPatternSet::kVoices) {
        if (value) patternSet.accents[stepIdx] = true;
      } else {
        patternSet.accents[stepIdx] = value;
      }
    }
    return;
  }

  if (path == Path::MuteDrums) {
    int muteIdx = stack_[stackSize_ - 1].index;
    if (muteIdx < 0 || muteIdx >= DrumPatternSet::kVoices) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid drum mute index %d\n", muteIdx);
      error_ = true;
      return;
    }
    drumMute_[muteIdx] = value;
    return;
  }

  if (path == Path::MuteSynth) {
    int muteIdx = stack_[stackSize_ - 1].index;
    if (muteIdx < 0 || muteIdx >= 2) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth mute index %d\n", muteIdx);
      error_ = true;
      return;
    }
    synthMute_[muteIdx] = value;
    return;
  }
  if (path == Path::SynthDistortion) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx < 0 || idx >= 2) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth distortion index %d\n", idx);
      error_ = true;
      return;
    }
    synthDistortion_[idx] = value;
    return;
  }
  if (path == Path::SynthDelay) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx < 0 || idx >= 2) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth delay index %d\n", idx);
      error_ = true;
      return;
    }
    synthDelay_[idx] = value;
    return;
  }

  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthSteps);
    bool useBankB = inSynthBankB();
    int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        stepIdx < 0 || stepIdx >= SynthPattern::kSteps ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      SCENE_DEBUG_PRINTF("SceneJsonObserver: invalid synth bool step=%d pattern=%d bank=%d\n",
                         stepIdx, patternIdx, bankIdx);
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                     : target_.synthABanks[bankIdx].patterns[patternIdx];
    if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value;
    }
    return;
  }
  if (path == Path::SynthAutomationLane && lastKey_ == "enabled") {
    automationEnabled_ = value;
    if (automationParam_ >= 0 && automationParam_ < static_cast<int>(TB303ParamId::Count)) {
      bool useBankB = inSynthBankB();
      int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
      if (bankIdx < 0) bankIdx = 0;
      int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
      if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
          bankIdx < 0 || bankIdx >= kBankCount) {
        return;
      }
      SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                       : target_.synthABanks[bankIdx].patterns[patternIdx];
      pattern.automation[automationParam_].enabled = automationEnabled_;
    }
    return;
  }
  if (path == Path::DrumAutomationLane && lastKey_ == "enabled") {
    drumAutomationEnabled_ = value;
    if (drumAutomationParam_ >= 0 &&
        drumAutomationParam_ < static_cast<int>(DrumAutomationParamId::Count)) {
      int bankIdx = currentIndexFor(Path::DrumBanks);
      if (bankIdx < 0) bankIdx = 0;
      int patternIdx = currentIndexFor(Path::DrumBank);
      if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
          bankIdx < 0 || bankIdx >= kBankCount) {
        return;
      }
      DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
      patternSet.automation[drumAutomationParam_].enabled = drumAutomationEnabled_;
    }
    return;
  }

  if (path == Path::State && lastKey_ == "songMode") {
    songMode_ = value;
  } else if (path == Path::State && lastKey_ == "loopMode") {
    loopMode_ = value;
  }
}

void SceneJsonObserver::onNumber(int value) { handlePrimitiveNumber(static_cast<double>(value), true); }

void SceneJsonObserver::onNumber(double value) { handlePrimitiveNumber(value, false); }

void SceneJsonObserver::onBool(bool value) { handlePrimitiveBool(value); }

void SceneJsonObserver::onNull() {}

void SceneJsonObserver::onString(const std::string& value) {
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::State && lastKey_ == "drumEngine") {
    drumEngineName_ = value;
    return;
  }
  if (path == Path::SynthAutomationOptions) {
    if (automationParam_ < 0 || automationParam_ >= static_cast<int>(TB303ParamId::Count)) return;
    bool useBankB = inSynthBankB();
    int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      return;
    }
    int optionIdx = stack_[stackSize_ - 1].index;
    if (optionIdx < 0 || optionIdx >= kAutomationMaxOptions) return;
    SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                     : target_.synthABanks[bankIdx].patterns[patternIdx];
    AutomationLane& lane = pattern.automation[automationParam_];
    lane.setOptionLabel(optionIdx, value.c_str());
    return;
  }
  if (path == Path::DrumAutomationOptions) {
    if (drumAutomationParam_ < 0 ||
        drumAutomationParam_ >= static_cast<int>(DrumAutomationParamId::Count)) {
      return;
    }
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      return;
    }
    int optionIdx = stack_[stackSize_ - 1].index;
    if (optionIdx < 0 || optionIdx >= kAutomationMaxOptions) return;
    DrumPatternSet& patternSet = target_.drumBanks[bankIdx].patterns[patternIdx];
    AutomationLane& lane = patternSet.automation[drumAutomationParam_];
    lane.setOptionLabel(optionIdx, value.c_str());
    return;
  }
}

void SceneJsonObserver::onObjectKey(const std::string& key) { lastKey_ = key; }

void SceneJsonObserver::onObjectValueStart() {}

void SceneJsonObserver::onObjectValueEnd() {
  if (error_) return;
  if (stackSize_ > 0 && stack_[stackSize_ - 1].type == Context::Type::Array) {
    ++stack_[stackSize_ - 1].index;
  }
}

bool SceneJsonObserver::hadError() const { return error_; }

int SceneJsonObserver::drumPatternIndex() const { return drumPatternIndex_; }

int SceneJsonObserver::synthPatternIndex(int synthIdx) const {
  int idx = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthPatternIndex_[idx];
}

int SceneJsonObserver::drumBankIndex() const { return drumBankIndex_; }

int SceneJsonObserver::synthBankIndex(int synthIdx) const {
  int idx = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthBankIndex_[idx];
}

bool SceneJsonObserver::drumMute(int idx) const {
  if (idx < 0) return drumMute_[0];
  if (idx >= DrumPatternSet::kVoices) return drumMute_[DrumPatternSet::kVoices - 1];
  return drumMute_[idx];
}

bool SceneJsonObserver::synthMute(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthMute_[clamped];
}

bool SceneJsonObserver::synthDistortionEnabled(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthDistortion_[clamped];
}

bool SceneJsonObserver::synthDelayEnabled(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthDelay_[clamped];
}

const SynthParameters& SceneJsonObserver::synthParameters(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthParameters_[clamped];
}

float SceneJsonObserver::bpm() const { return bpm_; }

const Song& SceneJsonObserver::song() const { return song_; }

bool SceneJsonObserver::hasSong() const { return hasSong_; }

bool SceneJsonObserver::songMode() const { return songMode_; }

int SceneJsonObserver::songPosition() const { return songPosition_; }

bool SceneJsonObserver::loopMode() const { return loopMode_; }

int SceneJsonObserver::loopStartRow() const { return loopStartRow_; }

int SceneJsonObserver::loopEndRow() const { return loopEndRow_; }

const std::string& SceneJsonObserver::drumEngineName() const { return drumEngineName_; }

void SceneManager::loadDefaultScene() {
  drumPatternIndex_ = 0;
  drumBankIndex_ = 0;
  synthPatternIndex_[0] = 0;
  synthPatternIndex_[1] = 0;
  synthBankIndex_[0] = 0;
  synthBankIndex_[1] = 0;
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) drumMute_[i] = false;
  synthMute_[0] = false;
  synthMute_[1] = false;
  synthDistortion_[0] = false;
  synthDistortion_[1] = false;
  synthDelay_[0] = false;
  synthDelay_[1] = false;
  synthParameters_[0] = SynthParameters();
  synthParameters_[1] = SynthParameters();
  drumEngineName_ = "808";
  setBpm(100.0f);
  songMode_ = false;
  songPosition_ = 0;
  loopMode_ = false;
  loopStartRow_ = 0;
  loopEndRow_ = 0;
  clearSongData(scene_.song);
  scene_.song.length = 1;
  scene_.song.positions[0].patterns[0] = 0;
  scene_.song.positions[0].patterns[1] = 0;
  scene_.song.positions[0].patterns[2] = 0;

  for (int b = 0; b < kBankCount; ++b) {
    for (int i = 0; i < Bank<DrumPatternSet>::kPatterns; ++i) {
      clearDrumPatternSet(scene_.drumBanks[b].patterns[i]);
    }
    for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
      clearSynthPattern(scene_.synthABanks[b].patterns[i]);
      clearSynthPattern(scene_.synthBBanks[b].patterns[i]);
    }
  }

  int8_t notes[SynthPattern::kSteps] = {48, 48, 55, 55, 50, 50, 55, 55,
                                        48, 48, 55, 55, 50, 55, 50, -1};
  bool accent[SynthPattern::kSteps] = {false, true, false, true, false, true,
                                       false, true, false, true, false, true,
                                       false, true, false, false};
  bool slide[SynthPattern::kSteps] = {false, true, false, true, false, true,
                                      false, true, false, true, false, true,
                                      false, true, false, false};

  int8_t notes2[SynthPattern::kSteps] = {48, 48, 55, 55, 50, 50, 55, 55,
                                         48, 48, 55, 55, 50, 55, 50, -1};
  bool accent2[SynthPattern::kSteps] = {true,  false, true,  false, true,  false,
                                        true,  false, true,  false, true,  false,
                                        true,  false, true,  false};
  bool slide2[SynthPattern::kSteps] = {false, false, true,  false, false, false,
                                       true,  false, false, false, true,  false,
                                       false, false, true,  false};

  bool kick[DrumPattern::kSteps] = {true,  false, false, false, true,  false,
                                    false, false, true,  false, false, false,
                                    true,  false, false, false};

  bool snare[DrumPattern::kSteps] = {false, false, true,  false, false, false,
                                     true,  false, false, false, true,  false,
                                     false, false, true,  false};

  bool hat[DrumPattern::kSteps] = {true, true, true, true, true, true, true, true,
                                   true, true, true, true, true, true, true, true};

  bool openHat[DrumPattern::kSteps] = {false, false, false, true,  false, false, false, false,
                                       false, false, false, true,  false, false, false, false};

  bool midTom[DrumPattern::kSteps] = {false, false, false, false, true,  false, false, false,
                                      false, false, false, false, true,  false, false, false};

  bool highTom[DrumPattern::kSteps] = {false, false, false, false, false, false, true,  false,
                                       false, false, false, false, false, false, true,  false};

  bool rim[DrumPattern::kSteps] = {false, false, false, false, false, true,  false, false,
                                   false, false, false, false, false, true,  false, false};

  bool clap[DrumPattern::kSteps] = {false, false, false, false, false, false, false, false,
                                    false, false, false, false, true,  false, false, false};

  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    scene_.synthABanks[0].patterns[0].steps[i].note = notes[i];
    scene_.synthABanks[0].patterns[0].steps[i].accent = accent[i];
    scene_.synthABanks[0].patterns[0].steps[i].slide = slide[i];

    scene_.synthBBanks[0].patterns[0].steps[i].note = notes2[i];
    scene_.synthBBanks[0].patterns[0].steps[i].accent = accent2[i];
    scene_.synthBBanks[0].patterns[0].steps[i].slide = slide2[i];
  }

  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    bool hatVal = hat[i];
    if (openHat[i]) {
      hatVal = false;
    }
    scene_.drumBanks[0].patterns[0].voices[0].steps[i].hit = kick[i];

    scene_.drumBanks[0].patterns[0].voices[1].steps[i].hit = snare[i];

    scene_.drumBanks[0].patterns[0].voices[2].steps[i].hit = hatVal;

    scene_.drumBanks[0].patterns[0].voices[3].steps[i].hit = openHat[i];

    scene_.drumBanks[0].patterns[0].voices[4].steps[i].hit = midTom[i];

    scene_.drumBanks[0].patterns[0].voices[5].steps[i].hit = highTom[i];

    scene_.drumBanks[0].patterns[0].voices[6].steps[i].hit = rim[i];

    scene_.drumBanks[0].patterns[0].voices[7].steps[i].hit = clap[i];
    scene_.drumBanks[0].patterns[0].accents[i] =
        kick[i] || snare[i] || hatVal || openHat[i] || midTom[i] || highTom[i] || rim[i] || clap[i];
  }
}

Scene& SceneManager::currentScene() { return scene_; }

const Scene& SceneManager::currentScene() const { return scene_; }

const DrumPatternSet& SceneManager::getCurrentDrumPattern() const {
  int bank = clampBankIndex(drumBankIndex_);
  return scene_.drumBanks[bank].patterns[clampPatternIndex(drumPatternIndex_)];
}

DrumPatternSet& SceneManager::editCurrentDrumPattern() {
  int bank = clampBankIndex(drumBankIndex_);
  return scene_.drumBanks[bank].patterns[clampPatternIndex(drumPatternIndex_)];
}

const SynthPattern& SceneManager::getCurrentSynthPattern(int synthIndex) const {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABanks[bank].patterns[patternIndex];
  }
  return scene_.synthBBanks[bank].patterns[patternIndex];
}

SynthPattern& SceneManager::editCurrentSynthPattern(int synthIndex) {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABanks[bank].patterns[patternIndex];
  }
  return scene_.synthBBanks[bank].patterns[patternIndex];
}

const SynthPattern& SceneManager::getSynthPattern(int synthIndex, int patternIndex) const {
  int idx = clampSynthIndex(synthIndex);
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABanks[bank].patterns[pat];
  }
  return scene_.synthBBanks[bank].patterns[pat];
}

SynthPattern& SceneManager::editSynthPattern(int synthIndex, int patternIndex) {
  int idx = clampSynthIndex(synthIndex);
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABanks[bank].patterns[pat];
  }
  return scene_.synthBBanks[bank].patterns[pat];
}

const DrumPatternSet& SceneManager::getDrumPatternSet(int patternIndex) const {
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(drumBankIndex_);
  return scene_.drumBanks[bank].patterns[pat];
}

DrumPatternSet& SceneManager::editDrumPatternSet(int patternIndex) {
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(drumBankIndex_);
  return scene_.drumBanks[bank].patterns[pat];
}

void SceneManager::setCurrentDrumPatternIndex(int idx) {
  drumPatternIndex_ = clampPatternIndex(idx);
}

void SceneManager::setCurrentSynthPatternIndex(int synthIdx, int idx) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthPatternIndex_[clampedSynth] = clampPatternIndex(idx);
}

int SceneManager::getCurrentDrumPatternIndex() const { return drumPatternIndex_; }

int SceneManager::getCurrentSynthPatternIndex(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthPatternIndex_[clampedSynth];
}

void SceneManager::setDrumMute(int voiceIdx, bool mute) {
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  drumMute_[clampedVoice] = mute;
}

bool SceneManager::getDrumMute(int voiceIdx) const {
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  return drumMute_[clampedVoice];
}

void SceneManager::setSynthMute(int synthIdx, bool mute) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthMute_[clampedSynth] = mute;
}

bool SceneManager::getSynthMute(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthMute_[clampedSynth];
}

void SceneManager::setSynthDistortionEnabled(int synthIdx, bool enabled) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthDistortion_[clampedSynth] = enabled;
}

bool SceneManager::getSynthDistortionEnabled(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthDistortion_[clampedSynth];
}

void SceneManager::setSynthDelayEnabled(int synthIdx, bool enabled) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthDelay_[clampedSynth] = enabled;
}

bool SceneManager::getSynthDelayEnabled(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthDelay_[clampedSynth];
}

void SceneManager::setSynthParameters(int synthIdx, const SynthParameters& params) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthParameters_[clampedSynth] = params;
}

const SynthParameters& SceneManager::getSynthParameters(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthParameters_[clampedSynth];
}

void SceneManager::setDrumEngineName(const std::string& name) { drumEngineName_ = name; }

const std::string& SceneManager::getDrumEngineName() const { return drumEngineName_; }

void SceneManager::setBpm(float bpm) {
  if (bpm < 40.0f) bpm = 40.0f;
  if (bpm > 200.0f) bpm = 200.0f;
  bpm_ = bpm;
}

float SceneManager::getBpm() const { return bpm_; }

const Song& SceneManager::song() const { return scene_.song; }

Song& SceneManager::editSong() { return scene_.song; }

void SceneManager::setSongPattern(int position, SongTrack track, int patternIndex) {
  int pos = position;
  if (pos < 0) pos = 0;
  if (pos >= Song::kMaxPositions) pos = Song::kMaxPositions - 1;
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return;
  int pat = clampSongPatternIndex(patternIndex);
  if (pos >= scene_.song.length) setSongLength(pos + 1);
  scene_.song.positions[pos].patterns[trackIdx] = static_cast<int8_t>(pat);
}

void SceneManager::clearSongPattern(int position, SongTrack track) {
  int pos = clampSongPosition(position);
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return;
  scene_.song.positions[pos].patterns[trackIdx] = -1;
  trimSongLength();
}

int SceneManager::songPattern(int position, SongTrack track) const {
  if (position < 0 || position >= Song::kMaxPositions) return -1;
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return -1;
  if (position >= scene_.song.length) return -1;
  return clampSongPatternIndex(scene_.song.positions[position].patterns[trackIdx]);
}

void SceneManager::setSongLength(int length) {
  int clamped = clampSongLength(length);
  scene_.song.length = clamped;
  if (songPosition_ >= scene_.song.length) songPosition_ = scene_.song.length - 1;
  if (songPosition_ < 0) songPosition_ = 0;
  clampLoopRange();
}

int SceneManager::songLength() const {
  int len = scene_.song.length;
  if (len < 1) len = 1;
  if (len > Song::kMaxPositions) len = Song::kMaxPositions;
  return len;
}

void SceneManager::setSongPosition(int position) {
  songPosition_ = clampSongPosition(position);
}

int SceneManager::getSongPosition() const {
  return clampSongPosition(songPosition_);
}

void SceneManager::setSongMode(bool enabled) { songMode_ = enabled; }

bool SceneManager::songMode() const { return songMode_; }

void SceneManager::setLoopMode(bool enabled) {
  loopMode_ = enabled;
  if (loopMode_) {
    clampLoopRange();
  }
}

bool SceneManager::loopMode() const { return loopMode_; }

void SceneManager::setLoopRange(int startRow, int endRow) {
  loopStartRow_ = startRow;
  loopEndRow_ = endRow;
  clampLoopRange();
}

int SceneManager::loopStartRow() const { return loopStartRow_; }

int SceneManager::loopEndRow() const { return loopEndRow_; }

void SceneManager::setCurrentBankIndex(int instrumentId, int bankIdx) {
  int clampedBank = clampBankIndex(bankIdx);
  if (instrumentId == 0) {
    drumBankIndex_ = clampedBank;
  } else {
    int synthIdx = clampSynthIndex(instrumentId - 1);
    synthBankIndex_[synthIdx] = clampedBank;
  }
}

int SceneManager::getCurrentBankIndex(int instrumentId) const {
  if (instrumentId == 0) return drumBankIndex_;
  int synthIdx = clampSynthIndex(instrumentId - 1);
  return synthBankIndex_[synthIdx];
}

void SceneManager::setDrumStep(int voiceIdx, int step, bool hit) {
  DrumPatternSet& patternSet = editCurrentDrumPattern();
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  int clampedStep = clampIndex(step, DrumPattern::kSteps);
  patternSet.voices[clampedVoice].steps[clampedStep].hit = hit;
}

void SceneManager::setSynthStep(int synthIdx, int step, int note, bool slide, bool accent) {
  SynthPattern& pattern = editCurrentSynthPattern(synthIdx);
  int clampedStep = clampIndex(step, SynthPattern::kSteps);
  pattern.steps[clampedStep].note = note;
  pattern.steps[clampedStep].slide = slide;
  pattern.steps[clampedStep].accent = accent;
}

std::string SceneManager::dumpCurrentScene() const {
  std::string serialized;
  writeSceneJson(serialized);
  return serialized;
}

bool SceneManager::loadScene(const std::string& json) {
  size_t idx = 0;
  JsonVisitor::NextChar nextChar = [&json, &idx]() -> int {
    if (idx >= json.size()) return -1;
    return static_cast<unsigned char>(json[idx++]);
  };
  return loadSceneEventedWithReader(nextChar);
}

bool SceneManager::loadSceneEventedWithReader(JsonVisitor::NextChar nextChar) {
  std::unique_ptr<Scene> loaded(new (std::nothrow) Scene());
  if (!loaded) {
    SCENE_DEBUG_PRINTLN("loadSceneEventedWithReader: failed to allocate Scene");
    return false;
  }
  clearSceneData(*loaded);

  struct NextCharStream {
    JsonVisitor::NextChar next;
    int read() { return next(); }
  };

  NextCharStream stream{std::move(nextChar)};
  JsonVisitor visitor;
  SceneJsonObserver observer(*loaded, bpm_);
  bool parsed = visitor.parse(stream, observer);
  if (!parsed || observer.hadError()) {
    SCENE_DEBUG_PRINTF("loadSceneEventedWithReader: parse=%s error=%s\n",
                       parsed ? "true" : "false",
                       observer.hadError() ? "true" : "false");
    return false;
  }

  scene_ = *loaded;
  scene_.song = observer.song();
  drumPatternIndex_ = clampPatternIndex(observer.drumPatternIndex());
  synthPatternIndex_[0] = clampPatternIndex(observer.synthPatternIndex(0));
  synthPatternIndex_[1] = clampPatternIndex(observer.synthPatternIndex(1));
  drumBankIndex_ = clampIndex(observer.drumBankIndex(), kBankCount);
  synthBankIndex_[0] = clampIndex(observer.synthBankIndex(0), kBankCount);
  synthBankIndex_[1] = clampIndex(observer.synthBankIndex(1), kBankCount);
  if (!observer.hasSong()) {
    scene_.song.length = 1;
    scene_.song.positions[0].patterns[0] = songPatternFromBank(synthBankIndex_[0],
                                                               synthPatternIndex_[0]);
    scene_.song.positions[0].patterns[1] = songPatternFromBank(synthBankIndex_[1],
                                                               synthPatternIndex_[1]);
    scene_.song.positions[0].patterns[2] = songPatternFromBank(drumBankIndex_,
                                                               drumPatternIndex_);
  }
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) {
    drumMute_[i] = observer.drumMute(i);
  }
  synthMute_[0] = observer.synthMute(0);
  synthMute_[1] = observer.synthMute(1);
  synthDistortion_[0] = observer.synthDistortionEnabled(0);
  synthDistortion_[1] = observer.synthDistortionEnabled(1);
  synthDelay_[0] = observer.synthDelayEnabled(0);
  synthDelay_[1] = observer.synthDelayEnabled(1);
  synthParameters_[0] = observer.synthParameters(0);
  synthParameters_[1] = observer.synthParameters(1);
  drumEngineName_ = observer.drumEngineName();
  setSongLength(scene_.song.length);
  songPosition_ = clampSongPosition(observer.songPosition());
  songMode_ = observer.songMode();
  loopMode_ = observer.loopMode();
  loopStartRow_ = observer.loopStartRow();
  loopEndRow_ = observer.loopEndRow();
  clampLoopRange();
  setBpm(observer.bpm());
  return true;
}

int SceneManager::clampPatternIndex(int idx) const {
  return clampIndex(idx, Bank<DrumPatternSet>::kPatterns);
}

int SceneManager::clampBankIndex(int idx) const {
  return clampIndex(idx, kBankCount);
}

int SceneManager::clampSynthIndex(int idx) const {
  if (idx < 0) return 0;
  if (idx > 1) return 1;
  return idx;
}

int SceneManager::clampSongPosition(int position) const {
  int len = songLength();
  if (len < 1) len = 1;
  if (position < 0) return 0;
  if (position >= len) return len - 1;
  if (position >= Song::kMaxPositions) return Song::kMaxPositions - 1;
  return position;
}

int SceneManager::clampSongLength(int length) const {
  if (length < 1) return 1;
  if (length > Song::kMaxPositions) return Song::kMaxPositions;
  return length;
}

int SceneManager::songTrackToIndex(SongTrack track) const {
  switch (track) {
  case SongTrack::SynthA: return 0;
  case SongTrack::SynthB: return 1;
  case SongTrack::Drums: return 2;
  default: return -1;
  }
}

void SceneManager::trimSongLength() {
  int lastUsed = -1;
  for (int pos = scene_.song.length - 1; pos >= 0; --pos) {
    bool hasData = false;
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      if (scene_.song.positions[pos].patterns[t] >= 0) {
        hasData = true;
        break;
      }
    }
    if (hasData) {
      lastUsed = pos;
      break;
    }
  }
  int newLength = lastUsed >= 0 ? lastUsed + 1 : 1;
  scene_.song.length = clampSongLength(newLength);
  if (songPosition_ >= scene_.song.length) songPosition_ = scene_.song.length - 1;
  clampLoopRange();
}

void SceneManager::clearSongData(Song& song) const {
  clearSong(song);
}

void SceneManager::clampLoopRange() {
  int len = songLength();
  int maxPos = len - 1;
  if (maxPos < 0) maxPos = 0;
  if (loopStartRow_ < 0) loopStartRow_ = 0;
  if (loopEndRow_ < 0) loopEndRow_ = 0;
  if (loopStartRow_ > maxPos) loopStartRow_ = maxPos;
  if (loopEndRow_ > maxPos) loopEndRow_ = maxPos;
  if (loopStartRow_ > loopEndRow_) {
    int tmp = loopStartRow_;
    loopStartRow_ = loopEndRow_;
    loopEndRow_ = tmp;
  }
}
