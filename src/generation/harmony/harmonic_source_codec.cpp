#include "harmonic_source_codec.h"

namespace GroovePuterHarmony {
namespace {
constexpr uint32_t kMagic = 0x53534748u;
constexpr uint32_t kFnvOffset = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;

class Writer {
 public:
  Writer(uint8_t* bytes, size_t capacity) : bytes_(bytes), capacity_(capacity) {}
  void byte(uint8_t value) {
    if (position_ >= capacity_) { valid_ = false; return; }
    if (bytes_ != nullptr) bytes_[position_] = value;
    ++position_;
  }
  void u16(uint16_t value) {
    byte(static_cast<uint8_t>(value));
    byte(static_cast<uint8_t>(value >> 8u));
  }
  void u32(uint32_t value) {
    u16(static_cast<uint16_t>(value));
    u16(static_cast<uint16_t>(value >> 16u));
  }
  void u64(uint64_t value) {
    u32(static_cast<uint32_t>(value));
    u32(static_cast<uint32_t>(value >> 32u));
  }
  size_t size() const { return position_; }
  bool valid() const { return valid_; }
 private:
  uint8_t* bytes_;
  size_t capacity_;
  size_t position_ = 0;
  bool valid_ = true;
};

class Reader {
 public:
  Reader(const uint8_t* bytes, size_t size) : bytes_(bytes), size_(size) {}
  uint8_t byte() {
    if (position_ >= size_) { valid_ = false; return 0; }
    return bytes_[position_++];
  }
  uint16_t u16() {
    const uint16_t low = byte();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(byte()) << 8u));
  }
  uint32_t u32() {
    const uint32_t low = u16();
    return low | (static_cast<uint32_t>(u16()) << 16u);
  }
  uint64_t u64() {
    const uint64_t low = u32();
    return low | (static_cast<uint64_t>(u32()) << 32u);
  }
  size_t consumed() const { return position_; }
  bool valid() const { return valid_; }
 private:
  const uint8_t* bytes_;
  size_t size_;
  size_t position_ = 0;
  bool valid_ = true;
};

uint32_t checksum(const uint8_t* bytes, size_t count) {
  uint32_t hash = kFnvOffset;
  for (size_t i = 0; i < count; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

void writeGenome(Writer& w, const SongVariantGenome& v) {
  w.byte(v.homeGravity); w.byte(v.harmonicMobility);
  w.byte(v.smoothnessBias); w.byte(v.commonToneBias);
  w.byte(v.functionalBias); w.byte(v.modalBias);
  w.byte(v.chromaticRisk); w.byte(v.modulationRisk);
  w.byte(v.portalRisk); w.byte(v.extensionComplexity);
  w.byte(v.verticalColor); w.byte(v.corpusConservatism);
  w.byte(v.repetitionTolerance); w.byte(v.cadenceStrength);
}
void readGenome(Reader& r, SongVariantGenome& v) {
  v.homeGravity = r.byte(); v.harmonicMobility = r.byte();
  v.smoothnessBias = r.byte(); v.commonToneBias = r.byte();
  v.functionalBias = r.byte(); v.modalBias = r.byte();
  v.chromaticRisk = r.byte(); v.modulationRisk = r.byte();
  v.portalRisk = r.byte(); v.extensionComplexity = r.byte();
  v.verticalColor = r.byte(); v.corpusConservatism = r.byte();
  v.repetitionTolerance = r.byte(); v.cadenceStrength = r.byte();
}
void writeState(Writer& w, const HarmonicState& v) {
  w.u16(v.qualityId); w.u16(v.pitchMask); w.byte(v.rootPc); w.byte(v.bassPc);
}
void readState(Reader& r, HarmonicState& v) {
  v.qualityId = r.u16(); v.pitchMask = r.u16(); v.rootPc = r.byte();
  v.bassPc = r.byte();
}
void writeContext(Writer& w, const TonalContext& v) {
  w.u16(v.scaleId); w.u16(v.homeScaleId); w.u16(v.scaleMask);
  w.u16(v.homeScaleMask); w.byte(v.tonicPc); w.byte(v.homeTonicPc);
}
void readContext(Reader& r, TonalContext& v) {
  v.scaleId = r.u16(); v.homeScaleId = r.u16(); v.scaleMask = r.u16();
  v.homeScaleMask = r.u16(); v.tonicPc = r.byte(); v.homeTonicPc = r.byte();
}
void writeEvent(Writer& w, const ResolvedHarmonicEvent& v) {
  writeState(w, v.state); writeContext(w, v.tonal); w.u16(v.relationTags);
  w.byte(static_cast<uint8_t>(v.cadenceClass)); w.byte(v.timingSlotOrdinal);
}
void readEvent(Reader& r, ResolvedHarmonicEvent& v) {
  readState(r, v.state); readContext(r, v.tonal); v.relationTags = r.u16();
  v.cadenceClass = static_cast<CadenceClass>(r.byte());
  v.timingSlotOrdinal = r.byte();
}
void writeFingerprint(Writer& w, const HarmonicFingerprintSet& v) {
  w.u64(v.rootMotion); w.u64(v.qualitySequence); w.u64(v.semanticRelation);
  w.u64(v.cadence); w.u64(v.scaleContext);
}
void readFingerprint(Reader& r, HarmonicFingerprintSet& v) {
  v.rootMotion = r.u64(); v.qualitySequence = r.u64();
  v.semanticRelation = r.u64(); v.cadence = r.u64();
  v.scaleContext = r.u64();
}

void writePayload(Writer& w, const SongHarmonicState& s) {
  w.u32(s.songIdentity); w.byte(s.songSlot); w.byte(s.startPosition);
  w.byte(s.rowCount); w.u32(s.projectSeed);
  w.byte(static_cast<uint8_t>(s.style)); w.u16(s.modelVersion);
  w.u64(s.corpusChecksum); writeGenome(w, s.genome);
  writeState(w, s.home); writeContext(w, s.homeTonal);
  w.u32(s.harmony.materialId); w.u32(s.harmony.publicationGeneration);
  w.u16(s.harmony.modelVersion);
  w.byte(static_cast<uint8_t>(s.harmony.authority));
  w.byte(s.harmony.eventCount);
  for (uint8_t i = 0; i < s.harmony.eventCount; ++i)
    writeEvent(w, s.harmony.events[i]);
  writeState(w, s.terminal); writeContext(w, s.terminalTonal);
  w.byte(s.recent.count); w.byte(s.recent.next);
  w.byte(s.recent.edgeCount); w.byte(s.recent.edgeNext);
  for (const auto& e : s.recent.entries) writeFingerprint(w, e);
  for (uint8_t e : s.recent.recentEdges) w.byte(e);
  w.u32(s.lastTransactionId); w.u32(s.lastTransactionMaterialId);
  w.byte(s.participantCount);
  for (uint8_t i = 0; i < s.participantCount; ++i) {
    const auto& p = s.participants[i];
    w.byte(p.track); w.byte(p.songPosition); w.byte(p.reserved);
    w.u32(p.materialId.value); w.u32(p.version.low); w.u32(p.version.high);
  }
}

void readPayload(Reader& r, SongHarmonicState& s) {
  s.songIdentity = r.u32(); s.songSlot = r.byte();
  s.startPosition = r.byte(); s.rowCount = r.byte(); s.projectSeed = r.u32();
  s.style = static_cast<HarmonicStyleId>(r.byte()); s.modelVersion = r.u16();
  s.corpusChecksum = r.u64(); readGenome(r, s.genome);
  readState(r, s.home); readContext(r, s.homeTonal);
  s.harmony.materialId = r.u32();
  s.harmony.publicationGeneration = r.u32();
  s.harmony.modelVersion = r.u16();
  s.harmony.authority = static_cast<HarmonicAuthority>(r.byte());
  s.harmony.eventCount = r.byte();
  if (s.harmony.eventCount > kMaxSongHarmonyEvents) return;
  for (uint8_t i = 0; i < s.harmony.eventCount; ++i)
    readEvent(r, s.harmony.events[i]);
  readState(r, s.terminal); readContext(r, s.terminalTonal);
  s.recent.count = r.byte(); s.recent.next = r.byte();
  s.recent.edgeCount = r.byte(); s.recent.edgeNext = r.byte();
  for (auto& e : s.recent.entries) readFingerprint(r, e);
  for (auto& e : s.recent.recentEdges) e = r.byte();
  s.lastTransactionId = r.u32(); s.lastTransactionMaterialId = r.u32();
  s.participantCount = r.byte();
  if (s.participantCount > kMaxSongHarmonyParticipants) return;
  for (uint8_t i = 0; i < s.participantCount; ++i) {
    auto& p = s.participants[i];
    p.track = r.byte(); p.songPosition = r.byte(); p.reserved = r.byte();
    p.materialId.value = r.u32(); p.version.low = r.u32();
    p.version.high = r.u32();
  }
}
}  // namespace

SongHarmonicCodecStatus encodeSongHarmonicState(
    const SongHarmonicState& state, uint8_t* destination, size_t capacity,
    size_t& written) {
  written = 0;
  if (destination == nullptr || !validSongHarmonicState(state))
    return SongHarmonicCodecStatus::InvalidInput;
  Writer measure(nullptr, kMaxSongHarmonicStateBytes);
  writePayload(measure, state);
  const size_t total = 8u + measure.size() + 4u;
  if (!measure.valid() || total > kMaxSongHarmonicStateBytes || total > 0xffffu)
    return SongHarmonicCodecStatus::InvalidData;
  if (capacity < total) return SongHarmonicCodecStatus::BufferTooSmall;
  Writer w(destination, capacity);
  w.u32(kMagic); w.u16(kSongHarmonicStateSchemaVersion);
  w.u16(static_cast<uint16_t>(total)); writePayload(w, state);
  if (!w.valid() || w.size() + 4u != total)
    return SongHarmonicCodecStatus::InvalidData;
  w.u32(checksum(destination, w.size()));
  if (!w.valid() || w.size() != total)
    return SongHarmonicCodecStatus::InvalidData;
  written = total;
  return SongHarmonicCodecStatus::Ok;
}

SongHarmonicCodecStatus decodeSongHarmonicState(
    const uint8_t* source, size_t size, SongHarmonicState& out) {
  if (source == nullptr || size < 12 || size > kMaxSongHarmonicStateBytes)
    return SongHarmonicCodecStatus::InvalidData;
  Reader h(source, 8);
  if (h.u32() != kMagic) return SongHarmonicCodecStatus::InvalidData;
  const uint16_t schema = h.u16();
  const uint16_t storedSize = h.u16();
  if (!h.valid() || storedSize != size) return SongHarmonicCodecStatus::InvalidData;
  if (schema != kSongHarmonicStateSchemaVersion)
    return SongHarmonicCodecStatus::UnsupportedVersion;
  Reader crcReader(source + size - 4u, 4);
  if (checksum(source, size - 4u) != crcReader.u32())
    return SongHarmonicCodecStatus::Corrupt;
  SongHarmonicState decoded{};
  Reader payload(source + 8u, size - 12u);
  readPayload(payload, decoded);
  if (!payload.valid() || payload.consumed() != size - 12u ||
      !validSongHarmonicState(decoded)) return SongHarmonicCodecStatus::InvalidData;
  out = decoded;
  return SongHarmonicCodecStatus::Ok;
}
}  // namespace GroovePuterHarmony
