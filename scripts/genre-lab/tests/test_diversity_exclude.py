"""Diversity with excluded steps: a fixed backbeat is genre, not a lack of variation."""
from dataclasses import replace

import pytest

from gplab.diversity import diversity
from gplab.schema import Bar
from gplab.synth import synth_takes

BACKBEAT = [[4, 0.8], [12, 0.8]]


def with_fixed_snare(bars):
    """Every take plays the same snare on the backbeat (steps 4 and 12)."""
    return [replace(b, events={**b.events, "snare": [list(e) for e in BACKBEAT]}) for b in bars]


def distinct_of(res):
    return {v: e["distinct"] for v, e in res.items()}


def test_stuck_generator_is_one_of_n_with_and_without_exclusion():
    bars = synth_takes("stuck", takes=8)
    plain = diversity(bars, roles=["kick", "snare", "hat", "ohat", "bass"])
    excl = diversity(bars, roles=["kick", "snare", "hat", "ohat", "bass"],
                     exclude={"kick": {0, 4, 8, 12}, "snare": {4, 12}})
    for v in plain:
        assert plain[v]["distinct"] == 1 and excl[v]["distinct"] == 1, v


def test_good_generator_with_fixed_backbeat_keeps_distinct_when_backbeat_is_excluded():
    bars = with_fixed_snare(synth_takes("good", takes=8))
    excl = diversity(bars, roles=["kick", "snare", "bass"],
                     exclude={"snare": {4, 12}, "kick": {0, 4, 8, 12}})
    # The backbeat is not counted: distinct must match the same takes with the snare role dropped.
    # Without the exclusion the fixed backbeat is a shared onset on every take, so the
    # exclusion is what keeps the number from being pulled down by a constant.
    no_snare = diversity(bars, roles=["kick", "bass"], exclude={"kick": {0, 4, 8, 12}})
    assert distinct_of(excl) == distinct_of(no_snare)
    for v, e in excl.items():
        assert e["distinct"] >= 4, (v, e)


def test_excluded_steps_are_reported():
    res = diversity(synth_takes("good", takes=2), roles=["snare"], exclude={"snare": {4, 12}})
    for e in res.values():
        assert e["excluded_steps"] == {"snare": [4, 12]}


@pytest.mark.parametrize("bad", [{"ride": {1}}, {"kick": {16}}])
def test_bad_exclusions_are_rejected(bad):
    with pytest.raises(ValueError):
        diversity(synth_takes("good", takes=2), exclude=bad)
