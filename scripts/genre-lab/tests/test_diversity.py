"""gplab diversity: distinct/N and mean_pair_hits per role, no profiles needed."""
import pytest

from gplab.diversity import diversity
from gplab.schema import ROLES
from gplab.synth import synth_takes


@pytest.mark.parametrize("role", ["kick", "snare", "hat", "ohat", "bass"])
def test_stuck_generator_has_one_distinct_take_per_role(role):
    res = diversity(synth_takes("stuck", takes=8), roles=[role])
    assert res, "no variants"
    for variant, e in res.items():
        assert e["takes"] == 8, variant
        assert e["distinct"] == 1, (variant, role, e)
        assert e["mean_pair_hits"][role] == 0.0, (variant, role)


def test_good_generator_reaches_four_of_eight_distinct():
    res = diversity(synth_takes("good", takes=8), roles=list(ROLES))
    for variant, e in res.items():
        assert e["takes"] == 8, variant
        assert e["distinct"] >= 4, (variant, e)


def test_unknown_role_is_rejected():
    with pytest.raises(ValueError, match="unknown roles"):
        diversity(synth_takes("good", takes=2), roles=["ride"])
