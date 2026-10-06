#!/usr/bin/env python3
"""Passes when every job a CI gate stands for passed or was skipped.

CI's required `arch-linux` check reads its jobs' results here rather than
leaving the verdict to GitHub, which skips a job whose dependency failed, and a
skipped required check lets a pull request merge (docs/development.md). A
skipped job passes, because the build jobs are skipped on a change that is
prose alone.

The needs context arrives through the environment rather than spliced into the
command, so nothing in it is read as shell:

    env:
      NEEDS: ${{ toJSON(needs) }}
    run: printf '%s' "$NEEDS" | scripts/check_job_results.py

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

    results = {job: need["result"] for job, need in needs.items()}
    for job, result in sorted(results.items()):
        print(f"{job}: {result}")
    failed = {job: result for job, result in results.items() if result not in PASSING}
    for job, result in sorted(failed.items()):
        print(f"::error::{job} did not pass: {result}", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
