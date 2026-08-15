#!/usr/bin/env python3
from dataclasses import dataclass
from enum import Enum
from typing import Iterable, List, Sequence
import unittest

REQUIRED_POOL_POLICIES_KIB = (8, 16, 24, 32, 48)
MIN_FRAGMENTATION_SWITCHES = 50


class AdmissionReason(str, Enum):
    OK = "ok"
    POOL_LIMIT = "pool-limit"
    TOTAL_HEAP = "total-heap"
    CONTIGUOUS_BLOCK = "contiguous-block"
    SLOT_LIMIT = "slot-limit"


class EvidenceClass(str, Enum):
    INCOMPLETE = "incomplete"
    PER_SAMPLE_HEALTHY = "per-sample-healthy"
    FRAGMENTATION_EVIDENCE = "fragmentation-evidence"
    GENERAL_MEMORY_PRESSURE = "general-memory-pressure"


@dataclass(frozen=True)
class MemorySnapshot:
    tag: str
    free_int: int
    largest_int: int
    free8: int
    largest8: int
    resident_pcm_bytes: int
    resident_sample_count: int
    largest_resident_sample: int
    pool_limit_bytes: int
    audio_underruns: int
    audio_cpu_peak_pct: float
    heap_integrity: bool


@dataclass(frozen=True)
class AllocationPlan:
    decoded_sizes: Sequence[int]
    pool_limit_bytes: int
    free8: int
    largest8: int
    resident_pcm_bytes: int = 0
    resident_sample_count: int = 0
    max_slots: int = 64

    @property
    def decoded_total(self) -> int:
        return sum(self.decoded_sizes)


@dataclass(frozen=True)
class FragmentationSample:
    switch_index: int
    free8: int
    largest8: int
    requested_allocation: int
    allocation_succeeded: bool
    heap_integrity: bool = True


@dataclass(frozen=True)
class MemoryCampaign:
    pool_policies_kib: Sequence[int]
    switch_samples: Sequence[FragmentationSample]
    snapshots: Sequence[MemorySnapshot]


def admit_plan(plan: AllocationPlan) -> AdmissionReason:
    if any(size <= 0 for size in plan.decoded_sizes):
        raise ValueError("decoded sample sizes must be positive")
    if plan.pool_limit_bytes <= 0 or plan.free8 < 0 or plan.largest8 < 0:
        raise ValueError("invalid memory limits")

    new_unique_bytes = plan.decoded_total
    if plan.resident_pcm_bytes + new_unique_bytes > plan.pool_limit_bytes:
        return AdmissionReason.POOL_LIMIT

    if plan.resident_sample_count + len(plan.decoded_sizes) > plan.max_slots:
        return AdmissionReason.SLOT_LIMIT

    if new_unique_bytes > plan.free8:
        return AdmissionReason.TOTAL_HEAP

    if max(plan.decoded_sizes) > plan.largest8:
        return AdmissionReason.CONTIGUOUS_BLOCK

    return AdmissionReason.OK


def validate_snapshot(snapshot: MemorySnapshot) -> None:
    integer_fields = (
        snapshot.free_int,
        snapshot.largest_int,
        snapshot.free8,
        snapshot.largest8,
        snapshot.resident_pcm_bytes,
        snapshot.resident_sample_count,
        snapshot.largest_resident_sample,
        snapshot.pool_limit_bytes,
        snapshot.audio_underruns,
    )
    if any(value < 0 for value in integer_fields):
        raise ValueError("negative memory telemetry")
    if snapshot.largest_int > snapshot.free_int:
        raise ValueError("largest_int exceeds free_int")
    if snapshot.largest8 > snapshot.free8:
        raise ValueError("largest8 exceeds free8")
    if snapshot.resident_pcm_bytes > snapshot.pool_limit_bytes:
        raise ValueError("resident PCM exceeds pool policy")
    if snapshot.resident_sample_count == 0 and snapshot.resident_pcm_bytes != 0:
        raise ValueError("PCM bytes without resident samples")
    if snapshot.audio_cpu_peak_pct < 0.0:
        raise ValueError("negative CPU telemetry")


def _has_required_pool_matrix(pool_policies_kib: Sequence[int]) -> bool:
    return all(policy in set(pool_policies_kib)
               for policy in REQUIRED_POOL_POLICIES_KIB)


def _fragmentation_failures(samples: Iterable[FragmentationSample]) -> List[FragmentationSample]:
    # This is the allocator-specific signature we care about: total heap says a
    # request should fit, but no contiguous block can satisfy it.
    return [
        sample for sample in samples
        if (not sample.allocation_succeeded
            and sample.free8 >= sample.requested_allocation
            and sample.largest8 < sample.requested_allocation)
    ]


def classify_campaign(campaign: MemoryCampaign) -> EvidenceClass:
    if not _has_required_pool_matrix(campaign.pool_policies_kib):
        return EvidenceClass.INCOMPLETE
    if len(campaign.switch_samples) < MIN_FRAGMENTATION_SWITCHES:
        return EvidenceClass.INCOMPLETE
    if not campaign.snapshots:
        return EvidenceClass.INCOMPLETE

    for snapshot in campaign.snapshots:
        validate_snapshot(snapshot)
    if any(not sample.heap_integrity for sample in campaign.switch_samples):
        return EvidenceClass.GENERAL_MEMORY_PRESSURE
    if any(not snapshot.heap_integrity for snapshot in campaign.snapshots):
        return EvidenceClass.GENERAL_MEMORY_PRESSURE

    frag = _fragmentation_failures(campaign.switch_samples)
    if frag:
        return EvidenceClass.FRAGMENTATION_EVIDENCE

    # A failure where total free heap itself is insufficient is pressure, not
    # evidence that per-sample allocation fragmented the heap.
    total_pressure = any(
        (not sample.allocation_succeeded
         and sample.free8 < sample.requested_allocation)
        for sample in campaign.switch_samples
    )
    if total_pressure:
        return EvidenceClass.GENERAL_MEMORY_PRESSURE

    return EvidenceClass.PER_SAMPLE_HEALTHY


class MemoryPolicyContractTests(unittest.TestCase):
    def test_same_total_pcm_can_have_different_contiguous_outcome(self) -> None:
        # Historical Tape cleanup evidence: ~38.3 KiB total free, ~21.5 KiB
        # largest block. The values are evidence, not a frozen 0.9.5 target.
        free8 = 38360
        largest8 = 21492
        pool = 32 * 1024

        many_small = AllocationPlan(
            decoded_sizes=[4 * 1024] * 8,
            pool_limit_bytes=pool,
            free8=free8,
            largest8=largest8,
        )
        one_large = AllocationPlan(
            decoded_sizes=[24 * 1024, 8 * 1024],
            pool_limit_bytes=pool,
            free8=free8,
            largest8=largest8,
        )

        self.assertEqual(many_small.decoded_total, one_large.decoded_total)
        self.assertEqual(admit_plan(many_small), AdmissionReason.OK)
        self.assertEqual(admit_plan(one_large), AdmissionReason.CONTIGUOUS_BLOCK)

    def test_48_kib_is_a_negative_admission_control_at_32_kib_pool(self) -> None:
        plan = AllocationPlan(
            decoded_sizes=[6 * 1024] * 8,
            pool_limit_bytes=32 * 1024,
            free8=64 * 1024,
            largest8=32 * 1024,
        )
        self.assertEqual(plan.decoded_total, 48 * 1024)
        self.assertEqual(admit_plan(plan), AdmissionReason.POOL_LIMIT)

    def test_total_heap_and_contiguous_failures_are_not_conflated(self) -> None:
        total_pressure = AllocationPlan(
            decoded_sizes=[12 * 1024],
            pool_limit_bytes=32 * 1024,
            free8=10 * 1024,
            largest8=10 * 1024,
        )
        fragmentation = AllocationPlan(
            decoded_sizes=[12 * 1024],
            pool_limit_bytes=32 * 1024,
            free8=20 * 1024,
            largest8=8 * 1024,
        )
        self.assertEqual(admit_plan(total_pressure), AdmissionReason.TOTAL_HEAP)
        self.assertEqual(admit_plan(fragmentation), AdmissionReason.CONTIGUOUS_BLOCK)

    def test_slot_admission_is_independent_from_byte_budget(self) -> None:
        plan = AllocationPlan(
            decoded_sizes=[512, 512],
            pool_limit_bytes=32 * 1024,
            free8=32 * 1024,
            largest8=16 * 1024,
            resident_sample_count=63,
            max_slots=64,
        )
        self.assertEqual(admit_plan(plan), AdmissionReason.SLOT_LIMIT)

    def test_snapshot_requires_consistent_heap_and_pool_telemetry(self) -> None:
        good = MemorySnapshot(
            tag="after-load",
            free_int=20000,
            largest_int=12000,
            free8=30000,
            largest8=16000,
            resident_pcm_bytes=16384,
            resident_sample_count=4,
            largest_resident_sample=4096,
            pool_limit_bytes=32768,
            audio_underruns=0,
            audio_cpu_peak_pct=22.0,
            heap_integrity=True,
        )
        validate_snapshot(good)

        with self.assertRaises(ValueError):
            validate_snapshot(MemorySnapshot(
                tag="bad",
                free_int=10000,
                largest_int=11000,
                free8=30000,
                largest8=16000,
                resident_pcm_bytes=0,
                resident_sample_count=0,
                largest_resident_sample=0,
                pool_limit_bytes=32768,
                audio_underruns=0,
                audio_cpu_peak_pct=10.0,
                heap_integrity=True,
            ))

    def test_campaign_is_incomplete_without_full_policy_matrix(self) -> None:
        campaign = MemoryCampaign(
            pool_policies_kib=[8, 16, 24, 32],
            switch_samples=[
                FragmentationSample(i, 30000, 20000, 4096, True)
                for i in range(50)
            ],
            snapshots=[self._baseline_snapshot()],
        )
        self.assertEqual(classify_campaign(campaign), EvidenceClass.INCOMPLETE)

    def test_campaign_is_incomplete_before_50_switches(self) -> None:
        campaign = MemoryCampaign(
            pool_policies_kib=REQUIRED_POOL_POLICIES_KIB,
            switch_samples=[
                FragmentationSample(i, 30000, 20000, 4096, True)
                for i in range(49)
            ],
            snapshots=[self._baseline_snapshot()],
        )
        self.assertEqual(classify_campaign(campaign), EvidenceClass.INCOMPLETE)

    def test_fragmentation_evidence_requires_free_total_but_small_largest_block(self) -> None:
        samples = [
            FragmentationSample(i, 30000, 18000, 8192, True)
            for i in range(49)
        ]
        samples.append(
            FragmentationSample(
                49,
                free8=26000,
                largest8=6000,
                requested_allocation=8192,
                allocation_succeeded=False,
            )
        )
        campaign = MemoryCampaign(
            pool_policies_kib=REQUIRED_POOL_POLICIES_KIB,
            switch_samples=samples,
            snapshots=[self._baseline_snapshot()],
        )
        self.assertEqual(
            classify_campaign(campaign),
            EvidenceClass.FRAGMENTATION_EVIDENCE,
        )

    def test_low_total_heap_is_pressure_not_arena_evidence(self) -> None:
        samples = [
            FragmentationSample(i, 30000, 18000, 8192, True)
            for i in range(49)
        ]
        samples.append(
            FragmentationSample(
                49,
                free8=7000,
                largest8=6500,
                requested_allocation=8192,
                allocation_succeeded=False,
            )
        )
        campaign = MemoryCampaign(
            pool_policies_kib=REQUIRED_POOL_POLICIES_KIB,
            switch_samples=samples,
            snapshots=[self._baseline_snapshot()],
        )
        self.assertEqual(
            classify_campaign(campaign),
            EvidenceClass.GENERAL_MEMORY_PRESSURE,
        )

    def test_clean_campaign_keeps_per_sample_as_default_hypothesis(self) -> None:
        campaign = MemoryCampaign(
            pool_policies_kib=REQUIRED_POOL_POLICIES_KIB,
            switch_samples=[
                FragmentationSample(
                    i,
                    free8=30000 - (i % 5) * 128,
                    largest8=18000 - (i % 3) * 128,
                    requested_allocation=8192,
                    allocation_succeeded=True,
                )
                for i in range(50)
            ],
            snapshots=[self._baseline_snapshot()],
        )
        self.assertEqual(
            classify_campaign(campaign),
            EvidenceClass.PER_SAMPLE_HEALTHY,
        )

    @staticmethod
    def _baseline_snapshot() -> MemorySnapshot:
        return MemorySnapshot(
            tag="runtime-baseline",
            free_int=26000,
            largest_int=16000,
            free8=38360,
            largest8=21492,
            resident_pcm_bytes=0,
            resident_sample_count=0,
            largest_resident_sample=0,
            pool_limit_bytes=32768,
            audio_underruns=0,
            audio_cpu_peak_pct=12.0,
            heap_integrity=True,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
