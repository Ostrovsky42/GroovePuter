#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path

TSV_NAME = "GF2_G4_C2_CONTRACT_CANDIDATES.tsv"
REPORT_NAME = "GF2_G4_C2_CONTRACT_CANDIDATES.md"
SUMMARY_NAME = "g4-c2-summary.txt"
TOP_LIMIT = 5

FIELDS = [
    "rank",
    "owner_mode",
    "owner_genre",
    "recipe_id",
    "recipe_name",
    "rows",
    "effective_candidate_count",
    "observed_archetype_count",
    "observed_archetypes",
    "fully_observed",
    "rhythm_family_count",
    "rhythm_families",
    "observed_bass_identity_count",
    "observed_bass_identities",
    "exact_proven_admission_alias",
    "alias_owner_mode",
    "alias_recipe_id",
    "ranking_tier",
    "score",
    "candidate_predicate",
    "evidence",
    "promotion_decision",
    "blocking_reason",
]


@dataclass(frozen=True)
class OwnerObservation:
    mode: int
    genre: str
    recipe_id: int
    recipe_name: str
    rows: int
    effective_candidate_count: int
    archetypes: tuple[int, ...]
    families: tuple[str, ...]
    bass_ids: tuple[int, ...]
    contract_status: str


@dataclass(frozen=True)
class RankedCandidate:
    observation: OwnerObservation
    alias: OwnerObservation | None
    tier: str
    tier_order: int
    score: int
    predicate: str
    evidence: str
    blocker: str


def die(message: str) -> None:
    print(f"G4_C2_RANKING_FAIL reason={message}", file=sys.stderr)
    raise SystemExit(1)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {
            "owner_mode",
            "owner_genre",
            "recipe_id",
            "recipe_name",
            "effective_candidate_count",
            "selected_archetype_id",
            "RhythmFamily",
            "bass_identity",
            "contract_status",
        }
        fields = set(reader.fieldnames or [])
        if not required.issubset(fields):
            die("census_schema")
        return list(reader)


def parse_int(value: str, field: str) -> int:
    try:
        return int(value)
    except ValueError:
        die(f"invalid_integer:{field}={value}")
    raise AssertionError("unreachable")


def aggregate(rows: list[dict[str, str]]) -> list[OwnerObservation]:
    grouped: dict[tuple[int, int], list[dict[str, str]]] = {}
    for row in rows:
        mode = parse_int(row["owner_mode"], "owner_mode")
        recipe = parse_int(row["recipe_id"], "recipe_id")
        grouped.setdefault((mode, recipe), []).append(row)

    owners: list[OwnerObservation] = []
    for (mode, recipe), group in sorted(grouped.items()):
        genres = {row["owner_genre"] for row in group}
        names = {row["recipe_name"] for row in group}
        statuses = {row["contract_status"] for row in group}
        candidate_counts = {
            parse_int(row["effective_candidate_count"], "effective_candidate_count")
            for row in group
        }
        if len(genres) != 1 or len(names) != 1:
            die(f"owner_label_drift:mode={mode}:recipe={recipe}")
        if len(statuses) != 1:
            die(f"contract_status_drift:mode={mode}:recipe={recipe}")
        if len(candidate_counts) != 1:
            die(f"candidate_count_drift:mode={mode}:recipe={recipe}")

        archetypes = tuple(sorted({parse_int(row["selected_archetype_id"], "selected_archetype_id") for row in group}))
        families = tuple(sorted({row["RhythmFamily"] for row in group}))
        bass_ids = tuple(sorted({parse_int(row["bass_identity"], "bass_identity") for row in group}))
        owners.append(
            OwnerObservation(
                mode=mode,
                genre=next(iter(genres)),
                recipe_id=recipe,
                recipe_name=next(iter(names)),
                rows=len(group),
                effective_candidate_count=next(iter(candidate_counts)),
                archetypes=archetypes,
                families=families,
                bass_ids=bass_ids,
                contract_status=next(iter(statuses)),
            )
        )
    return owners


def choose_alias(review: OwnerObservation, proven: list[OwnerObservation]) -> OwnerObservation | None:
    matches = [owner for owner in proven if owner.archetypes == review.archetypes]
    if not matches:
        return None
    matches.sort(key=lambda owner: (owner.mode, owner.recipe_id))
    return matches[0]


def classify(review: OwnerObservation, proven: list[OwnerObservation]) -> RankedCandidate:
    fully_observed = len(review.archetypes) == review.effective_candidate_count
    plural = review.effective_candidate_count >= 2
    single_family = len(review.families) == 1
    alias = choose_alias(review, proven)

    score = int(fully_observed) + int(plural) + int(single_family) + int(alias is not None)

    archetype_text = ",".join(str(value) for value in review.archetypes)
    family_text = ",".join(review.families)

    if alias is not None:
        tier = "A_PROVEN_ADMISSION_ALIAS"
        tier_order = 0
        predicate = (
            f"effective admitted archetype set={{{archetype_text}}}; "
            f"all RhythmFamily={review.families[0] if single_family else family_text}; admitted space plural"
        )
        evidence = (
            f"fully_observed={int(fully_observed)}; exact archetype-set alias to "
            f"mode={alias.mode},recipe={alias.recipe_id}; rows={review.rows}"
        )
        if review.bass_ids != alias.bass_ids:
            blocker = "OWNER_EQUIVALENCE_NOT_PROVEN;BASS_VOCABULARY_DIFFERS_FROM_PROVEN_ALIAS"
        else:
            blocker = "OWNER_EQUIVALENCE_NOT_PROVEN"
    elif fully_observed and plural and single_family:
        tier = "B_SINGLE_FAMILY_PLURAL"
        tier_order = 1
        predicate = (
            f"all effective admitted candidates are RhythmFamily={review.families[0]}; "
            f"admitted space remains plural ({review.effective_candidate_count} candidates)"
        )
        evidence = (
            f"observed all {len(review.archetypes)}/{review.effective_candidate_count} admitted archetypes; "
            f"one RhythmFamily across {review.rows} materialized rows"
        )
        blocker = "MUSICAL_NECESSITY_NOT_PROVEN"
    elif review.effective_candidate_count == 1:
        tier = "D_SINGLETON"
        tier_order = 3
        predicate = f"effective admitted set is singleton archetype={{{archetype_text}}}"
        evidence = f"fully_observed={int(fully_observed)}; rows={review.rows}; family={family_text}"
        blocker = "DEGENERATE_SINGLE_CANDIDATE_SPACE"
    else:
        tier = "C_MIXED_FAMILY_PLURAL"
        tier_order = 2
        predicate = (
            f"effective admitted set spans RhythmFamily={{{family_text}}}; "
            "a family-only predicate is insufficient"
        )
        evidence = (
            f"observed {len(review.archetypes)}/{review.effective_candidate_count} archetypes; "
            f"family_count={len(review.families)}; rows={review.rows}"
        )
        blocker = "FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD"

    return RankedCandidate(
        observation=review,
        alias=alias,
        tier=tier,
        tier_order=tier_order,
        score=score,
        predicate=predicate,
        evidence=evidence,
        blocker=blocker,
    )


def rank_candidates(owners: list[OwnerObservation]) -> list[RankedCandidate]:
    proven = [owner for owner in owners if owner.contract_status == "PROVEN"]
    review = [owner for owner in owners if owner.contract_status == "REVIEW_REQUIRED"]
    if len(review) != 30:
        die(f"review_owner_count:{len(review)}!=30")
    if not proven:
        die("no_proven_reference_owners")

    ranked = [classify(owner, proven) for owner in review]
    ranked.sort(
        key=lambda item: (
            item.tier_order,
            item.observation.effective_candidate_count,
            item.observation.mode,
            item.observation.recipe_id,
        )
    )
    return ranked


def row_for(rank: int, candidate: RankedCandidate) -> dict[str, str]:
    obs = candidate.observation
    alias = candidate.alias
    return {
        "rank": str(rank),
        "owner_mode": str(obs.mode),
        "owner_genre": obs.genre,
        "recipe_id": str(obs.recipe_id),
        "recipe_name": obs.recipe_name,
        "rows": str(obs.rows),
        "effective_candidate_count": str(obs.effective_candidate_count),
        "observed_archetype_count": str(len(obs.archetypes)),
        "observed_archetypes": ",".join(str(value) for value in obs.archetypes),
        "fully_observed": str(int(len(obs.archetypes) == obs.effective_candidate_count)),
        "rhythm_family_count": str(len(obs.families)),
        "rhythm_families": ",".join(obs.families),
        "observed_bass_identity_count": str(len(obs.bass_ids)),
        "observed_bass_identities": ",".join(str(value) for value in obs.bass_ids),
        "exact_proven_admission_alias": str(int(alias is not None)),
        "alias_owner_mode": "" if alias is None else str(alias.mode),
        "alias_recipe_id": "" if alias is None else str(alias.recipe_id),
        "ranking_tier": candidate.tier,
        "score": str(candidate.score),
        "candidate_predicate": candidate.predicate,
        "evidence": candidate.evidence,
        "promotion_decision": "BLOCKED",
        "blocking_reason": candidate.blocker,
    }


def write_tsv(path: Path, rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def write_report(path: Path, ranked: list[RankedCandidate]) -> None:
    top = ranked[:TOP_LIMIT]
    lines = [
        "# GF2 G4-C2 Contract Candidate Ranking",
        "",
        "## Boundary",
        "",
        "C2 does not promote contracts. It ranks existing REVIEW_REQUIRED owners by structural evidence only.",
        "Genre labels, recipe names, and weights are display metadata and do not affect ranking.",
        "A high rank is not a claim of musical correctness; every candidate remains BLOCKED until an explicit musical contract is justified and tested.",
        "",
        "## Ranking method",
        "",
        "Priority is lexicographic, not a musical quality score:",
        "",
        "1. exact admitted-archetype-set alias to an owner that already has PROVEN evidence;",
        "2. fully observed, plural admitted space contained in one RhythmFamily;",
        "3. fully observed plural spaces spanning multiple RhythmFamily values;",
        "4. singleton admitted spaces last because they cannot distinguish an intentional rule from a degenerate implementation choice.",
        "",
        "The `score` column is only the count of four evidence flags: fully observed, plural, single-family, exact proven alias.",
        "",
        "## Top five candidates",
        "",
    ]

    for rank, candidate in enumerate(top, start=1):
        obs = candidate.observation
        lines.extend(
            [
                f"### {rank}. {obs.genre} / {obs.recipe_name} (mode={obs.mode}, recipe={obs.recipe_id})",
                "",
                f"- Tier: `{candidate.tier}`",
                f"- Candidate predicate: {candidate.predicate}",
                f"- Evidence: {candidate.evidence}",
                f"- Observed bass IDs: {','.join(str(value) for value in obs.bass_ids)}",
                f"- Promotion: `BLOCKED` — `{candidate.blocker}`",
                "",
            ]
        )

    first = top[0]
    if first.alias is not None:
        lines.extend(
            [
                "## Highest-value next check",
                "",
                f"Rank 1 is structurally special: its admitted archetype set exactly matches the PROVEN reference owner at mode={first.alias.mode}, recipe={first.alias.recipe_id}.",
                "That supports a focused admission-equivalence test, but not inheritance of downstream bass semantics.",
                "The observed bass vocabulary differs, so a future checkpoint must keep rhythm admission and bass selection as separate claims.",
                "",
            ]
        )

    lines.extend(
        [
            "## Non-claims",
            "",
            "- C2 does not infer genre identity from a RhythmFamily label.",
            "- C2 does not use weights as evidence.",
            "- C2 does not turn repeated observation into musical necessity.",
            "- C2 does not change the I6/C1 census or any production generator behavior.",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def write_summary(path: Path, ranked: list[RankedCandidate]) -> tuple[int, int, int]:
    single_family_plural = sum(
        1
        for item in ranked
        if len(item.observation.archetypes) == item.observation.effective_candidate_count
        and item.observation.effective_candidate_count >= 2
        and len(item.observation.families) == 1
    )
    aliases = sum(1 for item in ranked if item.alias is not None)
    top_candidates = min(TOP_LIMIT, single_family_plural)
    line = (
        f"G4_C2_RANKING owners={len(ranked)} top_candidates={top_candidates} "
        f"single_family_plural={single_family_plural} exact_proven_admission_aliases={aliases} auto_promoted=0\n"
    )
    path.write_text(line, encoding="utf-8")
    return top_candidates, single_family_plural, aliases


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--census", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    if not args.census.is_file():
        die(f"missing_census:{args.census}")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    owners = aggregate(read_rows(args.census))
    ranked = rank_candidates(owners)
    rows = [row_for(rank, candidate) for rank, candidate in enumerate(ranked, start=1)]
    write_tsv(args.output_dir / TSV_NAME, rows)
    write_report(args.output_dir / REPORT_NAME, ranked)
    top_candidates, single_family_plural, aliases = write_summary(args.output_dir / SUMMARY_NAME, ranked)

    print(
        "G4_C2_RANKING_PASS "
        f"owners={len(ranked)} top_candidates={top_candidates} "
        f"single_family_plural={single_family_plural} exact_proven_admission_aliases={aliases} auto_promoted=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
