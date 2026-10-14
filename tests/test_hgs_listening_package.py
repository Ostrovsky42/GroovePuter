#!/usr/bin/env python3
import csv
import pathlib
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNNER = ROOT / "tests/run_hgs_listening_package.sh"


def main():
    with tempfile.TemporaryDirectory(prefix="hgs-listening-contract-") as tmp:
        output = pathlib.Path(tmp)
        result = subprocess.run(
            ["bash", str(RUNNER), "--seed-count", "1", "--out-dir", str(output)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if result.returncode:
            raise AssertionError(result.stdout)

        summary_path = output / "summary.tsv"
        trace_path = output / "traces.tsv"
        example_path = output / "ab_examples.tsv"
        assert summary_path.is_file() and trace_path.is_file() and example_path.is_file()
        with summary_path.open(newline="") as handle:
            summary = list(csv.DictReader(handle, delimiter="\t"))
        with trace_path.open(newline="") as handle:
            traces = list(csv.DictReader(handle, delimiter="\t"))
        with example_path.open(newline="") as handle:
            examples = list(csv.DictReader(handle, delimiter="\t"))

        assert len(summary) == 34  # 17 profiles x major/minor contexts
        route_keys = {
            (row["style"], row["context"], row["seed"], row["section"])
            for row in traces
        }
        assert len(route_keys) == 170  # one seed x five sections x two contexts x 17
        assert len({row["style"] for row in summary}) == 17
        assert {row["context"] for row in summary} == {"MAJOR", "MINOR"}
        assert all(int(row["seeds_requested"]) == 1 for row in summary)
        assert all(row["determinism"] == "PASS" for row in summary)
        assert all(row["home_valid"] == "YES" for row in summary)
        assert all(row["route_invariant_failures"] == "0" for row in summary)
        assert all(row["status"] not in {"INVALID", "UNKNOWN"} for row in traces)
        assert len(examples) == 17 * 2 * 1 * 4 * 2
        assert {row["pair"] for row in examples} == {"A_LEGACY", "B_GRAPH"}
        assert {row["timing_slot"] for row in examples} == {str(i) for i in range(4)}
        print("HGS listening package smoke contract: PASS")


if __name__ == "__main__":
    main()
