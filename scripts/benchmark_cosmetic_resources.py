#!/usr/bin/env python3
"""Measures the cosmetic blocking path across two revisions.

Each revision is built in a worktree of its own from the same sources, with the
same build options, the same pinned filter lists, and the same fixtures. The
harness in `tests/benchmarks` does the measuring; this script prepares the
builds, alternates the runs so that drift falls on both revisions equally, and
collects the samples.

Two passes run per fixture and condition. The counting pass records lookups,
call counts, CPU, and paint timings with no tracing overhead. The tracing pass
repeats a smaller run with Chromium tracing on and is read only for style
recalculation.
"""

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import signal
import statistics
import subprocess
import sys

FIXTURES = ("site-css", "scriptlets", "large-dom")
CONDITIONS = ("cold", "warm")
TRACE_CATEGORIES = "blink,devtools.timeline"
BENCHMARK_TARGET = "omaweb-cosmetic-benchmark"
STARTUP_SECONDS = 8
LOAD_SECONDS = 1.2
INCLUDE_LINE = 'include("${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks/CosmeticBenchmark.cmake")'


def run(command, **keywords):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(command, check=True, **keywords)


def run_measurement(command, environment, timeout, output):
    """Runs one measurement, and gives up on a process that will not exit.

    Shutting QtWebEngine down under tracing sometimes deadlocks after the trace
    has been written. The harness writes its samples before it waits, so a
    process killed here has still left everything the run needs behind.
    """
    print("+", " ".join(str(part) for part in command), flush=True)
    with subprocess.Popen(
        command, env=environment, stdout=subprocess.DEVNULL, start_new_session=True
    ) as process:
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            if not output.exists():
                raise
            print("  gave up on a stuck process; its samples were already written", flush=True)


def prepare(repository, revision, name, worktrees):
    """Builds one revision with the harness copied in, and returns its binary."""
    worktree = worktrees / name
    if not worktree.exists():
        run(["git", "-C", str(repository), "worktree", "add", "--detach", str(worktree), revision])
    shutil.copytree(
        repository / "tests" / "benchmarks", worktree / "tests" / "benchmarks", dirs_exist_ok=True
    )
    lists = worktree / "CMakeLists.txt"
    if INCLUDE_LINE not in lists.read_text():
        lists.write_text(lists.read_text().rstrip("\n") + "\n" + INCLUDE_LINE + "\n")
    run([sys.executable, str(worktree / "tests" / "benchmarks" / "instrument.py"), str(worktree)])
    run(["./scripts/bootstrap_content_blocker.sh"], cwd=worktree)
    run(["cmake", "--preset", "release", "-DBUILD_TESTING=ON"], cwd=worktree)
    run(["cmake", "--build", "--preset", "release", "--target", BENCHMARK_TARGET], cwd=worktree)
    return worktree / "build" / "release" / BENCHMARK_TARGET


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def first_line(command):
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
    except OSError:
        return None
    return (result.stdout or result.stderr).strip().splitlines()[:1] or None


def platform_facts(repository):
    """What the machine was, so a reader can judge how far the numbers travel."""
    baseline = json.loads((repository / "security" / "baseline.json").read_text())
    memory = {
        field.split(":")[0]: field.split(":")[1].strip()
        for field in pathlib.Path("/proc/meminfo").read_text().splitlines()[:3]
    }
    release = dict(
        line.split("=", 1)
        for line in pathlib.Path("/etc/os-release").read_text().splitlines()
        if "=" in line
    )
    return {
        "operatingSystem": release.get("PRETTY_NAME", "").strip('"'),
        "kernel": first_line(["uname", "-sr"]),
        "architecture": first_line(["uname", "-m"]),
        "virtualisation": first_line(["systemd-detect-virt"]),
        "compositor": first_line(["hyprctl", "version"]),
        "session": os.environ.get("XDG_SESSION_TYPE"),
        "processors": os.cpu_count(),
        "memory": memory,
        "qt": first_line(["qmake6", "-query", "QT_VERSION"]),
        "qtWebEngine": baseline["qtwebengine"],
        "chromium": baseline["chromium"],
        "rust": first_line(["rustc", "--version"]),
        "renderNodes": sorted(path.name for path in pathlib.Path("/dev/dri").glob("*")),
    }


def measure(
    binary, fixture, condition, lists, output, navigations, warmup, label, settle, trace=None
):
    environment = dict(os.environ)
    environment["QT_QPA_PLATFORM"] = "wayland"
    command = [
        str(binary),
        f"--fixture={fixture}",
        f"--condition={condition}",
        f"--lists={lists}",
        f"--output={output}",
        f"--navigations={navigations}",
        f"--warmup={warmup}",
        f"--settle={settle}",
        f"--label={label}",
    ]
    environment.pop("QTWEBENGINE_CHROMIUM_FLAGS", None)
    timeout = STARTUP_SECONDS + (warmup + navigations) * (settle / 1000.0 + LOAD_SECONDS) + 60
    if trace is not None:
        # Chromium writes a startup trace when its own timer runs out, so the
        # timer has to outlast the navigations and the process has to outlast
        # the timer.
        seconds = STARTUP_SECONDS + (warmup + navigations) * (settle / 1000.0 + LOAD_SECONDS)
        environment["QTWEBENGINE_CHROMIUM_FLAGS"] = (
            f"--trace-startup={TRACE_CATEGORIES} --trace-startup-format=json "
            f"--trace-startup-file={trace} --trace-startup-duration={int(seconds)}"
        )
        command.append(f"--hold={int(seconds) + 5}")
        timeout = seconds + 60
    run_measurement(command, environment, timeout, output)
    return json.loads(output.read_text())


def style_recalculations(trace, samples):
    """Attributes UpdateLayoutTree events to the navigation they fall inside.

    Chromium stamps trace events with CLOCK_MONOTONIC microseconds on Linux, the
    same clock the harness records its navigation boundaries on.
    """
    events = [
        event
        for event in json.loads(trace.read_text())["traceEvents"]
        if event.get("name") == "UpdateLayoutTree" and event.get("ph") == "X"
    ]
    attributed = []
    for sample in samples:
        start = sample["startedAtMicroseconds"]
        end = sample["finishedAtMicroseconds"]
        inside = [event for event in events if start <= event["ts"] <= end]
        attributed.append(
            {
                **sample,
                "styleRecalculations": len(inside),
                "styleRecalculationMilliseconds": sum(
                    event.get("tdur", event.get("dur", 0)) for event in inside
                )
                / 1000.0,
                "styleRecalculationElements": sum(
                    event.get("args", {}).get("elementCount", 0) for event in inside
                ),
            }
        )
    return attributed, len(events)


def summarise(values):
    values = [value for value in values if value is not None]
    if not values:
        return None
    ordered = sorted(values)
    return {
        "samples": len(ordered),
        "median": statistics.median(ordered),
        "p95": ordered[min(len(ordered) - 1, int(round(0.95 * (len(ordered) - 1))))],
        "minimum": ordered[0],
        "maximum": ordered[-1],
        "stdev": statistics.stdev(ordered) if len(ordered) > 1 else 0.0,
    }


METRICS = (
    ("lookups", lambda sample: sample["lookups"]),
    ("cosmeticScriptCalls", lambda sample: sample["cosmeticScriptCalls"]),
    ("surveyCallbacks", lambda sample: sample["surveyCallbacks"]),
    ("siteStyleMutations", lambda sample: sample["siteStyleMutations"]),
    ("blockerCallTotal", lambda sample: sum(sample["blockerCalls"].values())),
    ("cpuMilliseconds", lambda sample: sample["cpuMilliseconds"]),
    ("firstPaintMilliseconds", lambda sample: sample["firstPaintMilliseconds"]),
    (
        "firstContentfulPaintMilliseconds",
        lambda sample: sample["firstContentfulPaintMilliseconds"],
    ),
    (
        "largestContentfulPaintMilliseconds",
        lambda sample: sample["largestContentfulPaintMilliseconds"],
    ),
    ("styleRecalculations", lambda sample: sample.get("styleRecalculations")),
    (
        "styleRecalculationMilliseconds",
        lambda sample: sample.get("styleRecalculationMilliseconds"),
    ),
)


REPORT_METRICS = (
    ("lookups", "URL resource assemblies", ""),
    ("blockerCallTotal", "Cosmetic questions at the blocker boundary", ""),
    ("cosmeticScriptCalls", "Cosmetic scripts sent into the page", ""),
    ("surveyCallbacks", "Survey callbacks taken back", ""),
    ("siteStyleMutations", "Site stylesheet text mutations", ""),
    ("cpuMilliseconds", "CPU over the navigation window", " ms"),
    ("firstPaintMilliseconds", "First paint", " ms"),
    ("largestContentfulPaintMilliseconds", "Largest contentful paint", " ms"),
    ("styleRecalculations", "Style recalculations", ""),
    ("styleRecalculationMilliseconds", "Style recalculation CPU", " ms"),
)


def cell(summary, unit):
    if summary is None:
        return "unavailable"
    return (
        f"{summary['median']:g}{unit} / {summary['p95']:g}{unit} "
        f"(sd {summary['stdev']:.3g}, n={summary['samples']})"
    )


def write_report(results, path):
    """Medians, p95, variation, and sample counts, one table per fixture."""
    lines = [
        "# Cosmetic resource benchmark",
        "",
        f"Baseline `{results['revisions']['baseline'][:12]}` against implementation "
        f"`{results['revisions']['implementation'][:12]}`.",
        "",
        "Each cell reads median / p95 (standard deviation, sample count).",
        "",
    ]
    for run in results["runs"]:
        lines += [
            f"## {run['fixture']}, {run['condition']}",
            "",
            "| Metric | Baseline | Implementation |",
            "| --- | --- | --- |",
        ]
        for metric, title, unit in REPORT_METRICS:
            source = "trace" if metric.startswith("style") else "counts"
            before = run[source]["baseline"].get(metric)
            after = run[source]["implementation"].get(metric)
            lines.append(f"| {title} | {cell(before, unit)} | {cell(after, unit)} |")
        lines.append("")
    path.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--implementation", required=True)
    parser.add_argument("--lists", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--navigations", type=int, default=20)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--batch", type=int, default=5)
    parser.add_argument("--settle", type=int, default=1200)
    parser.add_argument("--trace-navigations", type=int, default=10)
    parser.add_argument("--fixtures", nargs="*", default=list(FIXTURES))
    parser.add_argument("--skip-build", action="store_true")
    arguments = parser.parse_args()

    repository = pathlib.Path(__file__).resolve().parent.parent
    out = arguments.out.resolve()
    (out / "samples").mkdir(parents=True, exist_ok=True)
    (out / "traces").mkdir(parents=True, exist_ok=True)
    worktrees = out / "worktrees"
    worktrees.mkdir(parents=True, exist_ok=True)

    revisions = {"baseline": arguments.baseline, "implementation": arguments.implementation}
    binaries = {}
    for name, revision in revisions.items():
        binary = worktrees / name / "build" / "release" / BENCHMARK_TARGET
        if not arguments.skip_build:
            binary = prepare(repository, revision, name, worktrees)
        binaries[name] = binary

    results = {
        "platform": platform_facts(repository),
        "revisions": {
            name: subprocess.run(
                ["git", "-C", str(repository), "rev-parse", revision],
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            for name, revision in revisions.items()
        },
        "lists": {
            "easylist": digest(arguments.lists / "easylist.txt"),
            "easyprivacy": digest(arguments.lists / "easyprivacy.txt"),
        },
        "runs": [],
    }

    for fixture in arguments.fixtures:
        for condition in CONDITIONS:
            collected = {name: [] for name in revisions}
            batch = 1 if condition == "cold" else arguments.batch
            rounds = max(1, arguments.navigations // batch)
            # Alternating rounds put any drift in the machine on both revisions.
            for round_index in range(rounds):
                for name in revisions:
                    label = f"{fixture}-{condition}-{name}-{round_index}"
                    path = out / "samples" / f"{label}.json"
                    report = measure(
                        binaries[name],
                        fixture,
                        condition,
                        arguments.lists,
                        path,
                        batch,
                        0 if condition == "cold" else arguments.warmup,
                        label,
                        arguments.settle,
                    )
                    collected[name].extend(
                        sample for sample in report["samples"] if not sample["warmup"]
                    )
                    results.setdefault("fixtureSizes", {})[fixture] = report["fixtureSize"]

            # A cold navigation is one process, so a cold trace needs as many
            # processes as it wants samples. A warm one traces a single process.
            traced = {name: [] for name in revisions}
            rounds = arguments.trace_navigations if condition == "cold" else 1
            for round_index in range(rounds):
                for name in revisions:
                    label = f"{fixture}-{condition}-{name}-trace-{round_index}"
                    trace = out / "traces" / f"{label}.json"
                    path = out / "samples" / f"{label}.json"
                    report = measure(
                        binaries[name],
                        fixture,
                        condition,
                        arguments.lists,
                        path,
                        1 if condition == "cold" else arguments.trace_navigations,
                        0 if condition == "cold" else arguments.warmup,
                        label,
                        arguments.settle,
                        trace=trace,
                    )
                    measured = [sample for sample in report["samples"] if not sample["warmup"]]
                    if not trace.exists():
                        print(f"{label}: no trace was written", flush=True)
                        continue
                    attributed, total = style_recalculations(trace, measured)
                    traced[name].extend(attributed)
                    print(f"{label}: {total} UpdateLayoutTree events in the trace", flush=True)

            results["runs"].append(
                {
                    "fixture": fixture,
                    "condition": condition,
                    "counts": {
                        name: {
                            metric: summarise([read(sample) for sample in collected[name]])
                            for metric, read in METRICS
                        }
                        for name in revisions
                    },
                    "trace": {
                        name: {
                            metric: summarise([read(sample) for sample in traced[name]])
                            for metric, read in METRICS
                            if metric.startswith("style")
                        }
                        for name in revisions
                    },
                }
            )
            (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")

    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    write_report(results, out / "report.md")
    print(f"wrote {out / 'results.json'} and {out / 'report.md'}")


if __name__ == "__main__":
    main()
