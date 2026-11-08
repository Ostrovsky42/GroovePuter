// D0-B test-local semantic model.
// This file proves the normative witnesses are mutually representable without
// choosing production enum names or layouts. It is not production architecture.

#include <cassert>
#include <cstdio>

namespace D0B {

enum class Lineage { Continues, NewIdea, Unknown };
enum class Reference { Source, Predecessor, ReturnTarget };
enum class StateRelation { Exact, Variation, Unknown, NotApplicable };
enum class Trajectory { None, Repeat, Development, Break, Return, Unknown };
enum class Genre { Allowed, Violation, Unknown };
enum class Operation { Honored, Violated, Unknown };
enum class Capability { Available, Unavailable, Unknown };

struct RelationFact {
  Reference reference;
  StateRelation result;
};

struct Facts {
  Lineage lineage = Lineage::Unknown;
  Trajectory trajectory = Trajectory::Unknown;
  Genre genre = Genre::Unknown;
  Operation operation = Operation::Unknown;
  Capability capability = Capability::Unknown;
  RelationFact relations[3] = {
      {Reference::Source, StateRelation::NotApplicable},
      {Reference::Predecessor, StateRelation::NotApplicable},
      {Reference::ReturnTarget, StateRelation::NotApplicable},
  };
};

StateRelation relation(const Facts& facts, Reference ref) {
  for (const auto& r : facts.relations) {
    if (r.reference == ref) return r.result;
  }
  return StateRelation::NotApplicable;
}

void b1_exact_immediate_repeat() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::Repeat;
  f.relations[1] = {Reference::Predecessor, StateRelation::Exact};
  assert(relation(f, Reference::Predecessor) == StateRelation::Exact);
  assert(f.lineage == Lineage::Continues);
  assert(f.trajectory == Trajectory::Repeat);
}

void b2_exact_return() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::Return;
  f.relations[2] = {Reference::ReturnTarget, StateRelation::Exact};
  assert(relation(f, Reference::ReturnTarget) == StateRelation::Exact);
  assert(f.trajectory == Trajectory::Return);
}

void b3_transformed_return() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::Return;
  f.relations[0] = {Reference::Source, StateRelation::Variation};
  f.relations[1] = {Reference::Predecessor, StateRelation::Variation};
  f.relations[2] = {Reference::ReturnTarget, StateRelation::Variation};
  assert(relation(f, Reference::Source) == StateRelation::Variation);
  assert(relation(f, Reference::Predecessor) == StateRelation::Variation);
  assert(relation(f, Reference::ReturnTarget) == StateRelation::Variation);
  assert(f.lineage == Lineage::Continues);
  assert(f.trajectory == Trajectory::Return);
}

void b4_isolated_variation() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::None;
  f.relations[0] = {Reference::Source, StateRelation::Variation};
  assert(relation(f, Reference::Source) == StateRelation::Variation);
  assert(f.trajectory == Trajectory::None);
}

void b5_directional_development() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::Development;
  f.relations[0] = {Reference::Source, StateRelation::Variation};
  f.relations[1] = {Reference::Predecessor, StateRelation::Variation};
  assert(f.trajectory == Trajectory::Development);
  assert(f.lineage == Lineage::Continues);
}

void b6_random_drift_is_not_development() {
  Facts f{};
  f.lineage = Lineage::Continues;
  f.trajectory = Trajectory::Unknown;
  f.relations[0] = {Reference::Source, StateRelation::Variation};
  f.relations[1] = {Reference::Predecessor, StateRelation::Variation};
  assert(f.trajectory != Trajectory::Development);
}

void b8_unavailable_capability_is_not_other_failure() {
  Facts f{};
  f.capability = Capability::Unavailable;
  f.genre = Genre::Unknown;
  f.operation = Operation::Unknown;
  assert(f.capability == Capability::Unavailable);
  assert(f.genre != Genre::Violation);
  assert(f.operation != Operation::Violated);
}

void b9_operation_violation_is_independent_of_genre() {
  Facts f{};
  f.operation = Operation::Violated;
  f.genre = Genre::Allowed;
  assert(f.operation == Operation::Violated);
  assert(f.genre == Genre::Allowed);
}

void b10_genre_valid_new_idea() {
  Facts f{};
  f.lineage = Lineage::NewIdea;
  f.genre = Genre::Allowed;
  assert(f.lineage == Lineage::NewIdea);
  assert(f.genre == Genre::Allowed);
}

void magnitude_falsification_pair() {
  Facts manyChanges{};
  manyChanges.lineage = Lineage::Continues;
  manyChanges.trajectory = Trajectory::None;

  Facts oneSmallStructuralChange{};
  oneSmallStructuralChange.lineage = Lineage::Continues;
  oneSmallStructuralChange.trajectory = Trajectory::Development;

  assert(manyChanges.trajectory != Trajectory::Development);
  assert(oneSmallStructuralChange.trajectory == Trajectory::Development);
}

}  // namespace D0B

int main() {
  D0B::b1_exact_immediate_repeat();
  D0B::b2_exact_return();
  D0B::b3_transformed_return();
  D0B::b4_isolated_variation();
  D0B::b5_directional_development();
  D0B::b6_random_drift_is_not_development();
  D0B::b8_unavailable_capability_is_not_other_failure();
  D0B::b9_operation_violation_is_independent_of_genre();
  D0B::b10_genre_valid_new_idea();
  D0B::magnitude_falsification_pair();
  std::puts("D0-B test-local semantic factorization: PASS");
  return 0;
}
