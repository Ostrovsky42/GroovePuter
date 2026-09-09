#include <cstdint>
#include <cstdio>

namespace {

enum class FaultKind : uint8_t {
  IncompleteOwnerEnumeration = 0,
  UnexplainedRawEffectiveMismatch,
  DuplicateMaterializationRow,
  UnknownArchetype,
  SelectionOutsideAdmission,
  WitnessDetectorError,
  ReviewRequiredPromotedToPass,
  RetainedControlRegression,
};

const char* faultName(FaultKind fault) {
  switch (fault) {
    case FaultKind::IncompleteOwnerEnumeration:
      return "INCOMPLETE_OWNER_ENUMERATION";
    case FaultKind::UnexplainedRawEffectiveMismatch:
      return "RAW_EFFECTIVE_MISMATCH";
    case FaultKind::DuplicateMaterializationRow:
      return "DUPLICATE_MATERIALIZATION_ROW";
    case FaultKind::UnknownArchetype:
      return "UNKNOWN_ARCHETYPE";
    case FaultKind::SelectionOutsideAdmission:
      return "SELECTION_OUTSIDE_ADMISSION";
    case FaultKind::WitnessDetectorError:
      return "WITNESS_DETECTOR_ERROR";
    case FaultKind::ReviewRequiredPromotedToPass:
      return "REVIEW_REQUIRED_PROMOTED_TO_PASS";
    case FaultKind::RetainedControlRegression:
      return "RETAINED_CONTROL_REGRESSION";
  }
  return "UNKNOWN_FAULT";
}

// Phase-1 RED placeholder. The fixtures below define the faults the measurement
// layer must reject. The initial detector deliberately recognizes none of them;
// the first CI run must therefore fail at runtime rather than at compile/link.
bool measurementRejects(FaultKind) { return false; }

bool runSelfTest(FaultKind fault) {
  if (measurementRejects(fault)) {
    std::printf("G4_I6_SELFTEST_PASS fault=%s\n", faultName(fault));
    return true;
  }
  std::printf("G4_I6_SELFTEST_FAIL fault=%s\n", faultName(fault));
  return false;
}

}  // namespace

int main() {
  constexpr FaultKind faults[] = {
      FaultKind::IncompleteOwnerEnumeration,
      FaultKind::UnexplainedRawEffectiveMismatch,
      FaultKind::DuplicateMaterializationRow,
      FaultKind::UnknownArchetype,
      FaultKind::SelectionOutsideAdmission,
      FaultKind::WitnessDetectorError,
      FaultKind::ReviewRequiredPromotedToPass,
      FaultKind::RetainedControlRegression,
  };

  int failures = 0;
  for (FaultKind fault : faults) {
    if (!runSelfTest(fault)) ++failures;
  }
  if (failures != 0) {
    std::printf("G4-I6 ownership census self-test: RED (%d failures)\n", failures);
    return 1;
  }
  std::puts("G4-I6 ownership census self-test: PASS");
  return 0;
}
