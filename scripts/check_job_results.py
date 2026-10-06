#!/usr/bin/env python3
"""Passes when every job a CI gate stands for passed or was skipped.

`arch-linux` is the check branch protection requires, and the jobs it stands
for run beside one another rather than as its steps. GitHub skips a job whose
dependency failed rather than failing it, and a skipped required check lets a
pull request merge, so the gate runs whatever its jobs did and reads their
results here. A skipped job passes: the build jobs are skipped on a change that
is prose alone, and that has to merge as it did before.

    echo '${{ toJSON(needs) }}' | scripts/check_job_results.py

Exits 0 when each job succeeded or was skipped, 1 when one failed or was
cancelled, or when there are no jobs to read.
"""

import json
import sys

PASSING = {"success", "skipped"}


def main():
    needs = json.load(sys.stdin)
    if not needs:
        print("check_job_results: no jobs to read", file=sys.stderr)
        return 1

    failed = {job: need["result"] for job, need in needs.items() if need["result"] not in PASSING}
    for job, result in sorted(needs.items()):
        print(f"{job}: {result['result']}")
    for job, result in sorted(failed.items()):
        print(f"::error::{job} did not pass: {result}", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
