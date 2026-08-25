#pragma once

#include <cstdint>

#include "e3_listen_review_hook.h"

class MiniAcid;

namespace GroovePuterRhythm {

enum class E3ListenAudibilityClass : uint8_t {
  ProductionContextAudition = 0,
};

struct E3ListenCaseInfo {
  const char* caseId = "";
  const char* category = "";
  const char* family = "";
  const char* level = "";
  const char* operation = "";
  const char* role = "";
  uint8_t roleIndex = 0;
  uint8_t sourceStep = 0xFFu;
  uint8_t targetStep = 0xFFu;
  const char* sourceClass = "";
  const char* sourceKind = "";
  uint8_t distance = 0;
  uint8_t densityBefore = 0;
  uint8_t densityAfter = 0;
  bool mutatedRoleExact = false;
  E3ListenAudibilityClass audibilityClass =
      E3ListenAudibilityClass::ProductionContextAudition;
};

uint8_t e3ListenCaseCount();
E3ListenCaseInfo e3ListenCaseInfo(uint8_t index);

bool applyE3ListenCase(MiniAcid& engine,
                       uint8_t index,
                       E3ListenVariant variant);

const char* e3ListenVariantName(E3ListenVariant variant);
const char* e3ListenAudibilityClassName(E3ListenAudibilityClass value);

}  // namespace GroovePuterRhythm
