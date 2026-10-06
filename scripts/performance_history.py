"""The performance history: one line per run, and the page that draws it.

Two harnesses write here. `scripts/benchmark_runtime.py --record` appends the runtime budget's
measurements, and `scripts/benchmark_chromium.py --record` appends a comparison against Chromium.
`performance/budget.json` keeps only the last recording, which says whether a number is under its
ceiling today and nothing about whether it is drifting towards it.

Each line stands on its own: the date, the Omaweb commit, the engine, the machine and every number
the run took, so a line copied out of the file still says what it measured. Runs from different
machines share the file and are told apart by their `machine` field, which is why nothing here
compares a line with the one above it without first asking whether both came from the same machine.

The page `plot` writes is a report rather than product interface: inline SVG, no script and nothing
fetched, so it opens from a file and can be attached to an issue as it is.
"""

from __future__ import annotations

import datetime
import html
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HISTORY = ROOT / "performance" / "history.jsonl"

# What each kind of line is. A reader of the file filters on it, and a line of a kind this does not
# know is kept and not drawn, so an older plot does not fall over on a newer file.
KIND_BUDGET = "budget"
KIND_COMPARISON = "comparison"

# The browsers of a comparison, in the order a report names them. `latest` is the stable Chromium
# a reader of Arch has; `matched` is the Chromium major the engine under test is built on.
OMAWEB = "omaweb"
LATEST = "latest"
MATCHED = "matched"
BASELINES = (LATEST, MATCHED)

BASELINE_NAMES = {
    LATEST: "latest stable Chromium",
    MATCHED: "Chromium the engine is based on",
}

ENGINE_LIBRARY = re.compile(r"libQt6WebEngineCore\.so[.\d]*")


def now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00", "Z")


def _run(command: list[str]) -> str:
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.strip() if result.returncode == 0 else ""


def describe_machine() -> str:
    """The machine as a reader would tell it apart from another: architecture, processor, memory.

    Not the host name, which says nothing about speed and is the reader's own business. A guest
    says it is one, because a virtual machine's numbers move with its host's load and a reader of
    the history has to know which lines can.
    """
    model = ""
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as handle:
            for line in handle:
                if line.lower().startswith("model name"):
                    model = line.split(":", 1)[1].strip()
                    break
    except OSError:
        pass
    if not model:
        # An Arm guest has no model name, and `lscpu` prints a dash for it; the vendor is what
        # there is to say.
        fields = dict(line.split(":", 1) for line in _run(["lscpu"]).splitlines() if ":" in line)
        for key in ("Model name", "Vendor ID"):
            value = fields.get(key, "").strip()
            if value and value != "-":
                model = value
                break
    parts = [platform.machine(), model or "unknown processor", f"{os.cpu_count()} CPUs"]
    try:
        with open("/proc/meminfo", encoding="utf-8") as handle:
            kib = int(handle.readline().split()[1])
        parts.append(f"{round(kib / 1024 / 1024)} GiB")
    except (OSError, IndexError, ValueError):
        pass
    virtualization = _run(["systemd-detect-virt"])
    if virtualization and virtualization != "none":
        parts.append(f"{virtualization} guest")
    return ", ".join(parts)


def read_version(executable: str) -> dict:
    """What `omaweb --version` says about itself and the engine it linked."""
    report = _run([executable, "--version"])
    found = {}
    match = re.search(r"^Omaweb (\S+)", report, re.MULTILINE)
    if match:
        found["omaweb"] = match.group(1)
    match = re.search(r"QtWebEngine (\S+), Chromium (\S+)", report)
    if match:
        found["qtwebengine"], found["chromium"] = match.group(1), match.group(2)
    return found


def owning_package(path: str) -> tuple[str, str]:
    """The pacman package that installed a file, and its version, or two blanks off a pacman
    system or for a file no package owns, such as a development build's own engine."""
    if not path or shutil.which("pacman") is None:
        return "", ""
    name = _run(["pacman", "-Qqo", path])
    if not name:
        return "", ""
    version = _run(["pacman", "-Q", name]).split()
    return name, version[1] if len(version) > 1 else ""


def linked_engine(executable: str) -> str:
    """The engine library the dynamic linker would give this executable.

    `ldd` resolves the executable's run path the way the launch does, which is how Omaweb's own
    engine under `/usr/lib/omaweb` is found ahead of Arch's.
    """
    for line in _run(["ldd", executable]).splitlines():
        if ENGINE_LIBRARY.search(line) and "=>" in line:
            return os.path.realpath(line.split("=>", 1)[1].split("(")[0].strip())
    return ""


def describe_engine(executable: str, library: str = "") -> dict:
    """The engine a browser runs on: its library, the package that owns it, and its versions.

    `library` is what a running browser was seen to have loaded, when the caller has one; otherwise
    it is read off the executable.
    """
    library = library or linked_engine(executable)
    package, package_version = owning_package(library)
    versions = read_version(executable)
    return {
        "library": library,
        "package": package,
        "version": package_version,
        "qtwebengine": versions.get("qtwebengine", ""),
        "chromium": versions.get("chromium", ""),
    }


def omaweb_commit(executable: str, version: str) -> str:
    """The commit the measured browser was built from, as well as this checkout can tell.

    A build inside this checkout is its `HEAD`. An installed package is the release tag of its
    version, when this checkout has the tag. Anything else is left blank rather than guessed.
    """
    resolved = os.path.realpath(executable)
    if resolved.startswith(str(ROOT) + os.sep):
        return _run(["git", "-C", str(ROOT), "rev-parse", "HEAD"])
    if version:
        return _run(["git", "-C", str(ROOT), "rev-parse", "--verify", "--quiet",
                     f"v{version}^{{commit}}"])
    return ""


def budget_record(measurements: dict, budget: dict, *, date: str, commit: str, machine: str,
                  engine: dict, omaweb: str) -> dict:
    """One budget run as a history line, each value beside the ceiling it was held to then.

    The ceiling travels with the value because ceilings are moved by review, and a value drawn
    against today's ceiling would have been measured against a different one.
    """
    thresholds = budget["measurements"]
    return {
        "kind": KIND_BUDGET,
        "date": date,
        "commit": commit,
        "omaweb": omaweb,
        "machine": machine,
        "engine": engine,
        "measurements": {
            name: {"value": round(value, 3), "ceiling": thresholds[name]["ceiling"]}
            for name, value in measurements.items()
        },
    }


def append(record: dict, path: Path = HISTORY) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, sort_keys=True) + "\n")


def read(path: Path = HISTORY) -> list[dict]:
    """Every line, oldest first. A line that is not JSON fails loudly: a history that silently
    lost a run would draw a line through the gap as though nothing had been there."""
    if not path.exists():
        return []
    records = []
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError as error:
                raise ValueError(f"{path}:{number} is not a history line: {error}") from error
    return sorted(records, key=lambda record: record["date"])


# The comparison's arithmetic.


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else float("nan")


def spread(values: list[float]) -> float:
    """The range of a browser's runs as a share of their mean: how much the host moved."""
    if len(values) < 2:
        return 0.0
    return (max(values) - min(values)) / mean(values)


def ratio(omaweb: list[float], chromium: list[float]) -> float:
    """Omaweb's score over Chromium's, each the mean of its runs. Every suite here scores bigger
    as better, so a ratio under one is Omaweb behind by that much.

    A ratio of means rather than a mean of ratios, because the runs alternate rather than pair: a
    run of each is two neighbours, not the same thing measured twice.
    """
    if not omaweb or not chromium:
        return float("nan")
    return mean(omaweb) / mean(chromium)


def comparison_ratios(record: dict) -> dict[str, dict[str, float]]:
    """Each suite's ratio against each baseline the record measured."""
    ratios: dict[str, dict[str, float]] = {}
    for suite, scores in record["scores"].items():
        for baseline in BASELINES:
            if scores.get(OMAWEB) and scores.get(baseline):
                ratios.setdefault(suite, {})[baseline] = ratio(scores[OMAWEB], scores[baseline])
    return ratios


# The plot's data.


def engine_label(engine: dict) -> str:
    package = engine.get("package") or "engine"
    version = engine.get("version") or engine.get("qtwebengine") or "unknown"
    # The same package version has been built with two toolchains, only one of them published.
    toolchain = engine.get("toolchain")
    return f"{package} {version}, {toolchain}" if toolchain else f"{package} {version}"


def engine_updates(records: list[dict]) -> list[dict]:
    """Where each machine's engine changed from its previous line, as marks on the time axis.

    The version-lag gap should jump at these, and the matched baseline changes Chromium version at
    them, so they are drawn on every chart. The first line of a machine is its start, not an
    update.
    """
    marks = []
    previous: dict[str, str] = {}
    for record in records:
        label = engine_label(record.get("engine", {}))
        machine = record.get("machine", "")
        if machine in previous and previous[machine] != label:
            marks.append({"date": record["date"], "machine": machine, "engine": label})
        previous[machine] = label
    return marks


def _scores_text(scores: dict[str, list[float]]) -> str:
    return "; ".join(f"{browser} " + ", ".join(f"{value:g}" for value in values)
                     for browser, values in scores.items())


def comparison_series(records: list[dict]) -> dict[str, list[dict]]:
    """For each suite, one series per machine and baseline, each point a run's ratio.

    A point carries its raw scores and the Chromium version it was measured against, because the
    matched baseline moves version with the engine and a ratio without its denominator's version
    reads as the same comparison when it is not.
    """
    charts: dict[str, dict[tuple[str, str], dict]] = {}
    for record in records:
        if record.get("kind") != KIND_COMPARISON:
            continue
        ratios = comparison_ratios(record)
        for suite, by_baseline in ratios.items():
            for baseline, value in by_baseline.items():
                key = (record["machine"], baseline)
                series = charts.setdefault(suite, {}).setdefault(
                    key, {"machine": record["machine"], "baseline": baseline, "points": []})
                browser = record["browsers"].get(baseline, {})
                series["points"].append({
                    "date": record["date"],
                    "value": value,
                    "chromium": browser.get("version", ""),
                    "engine": engine_label(record.get("engine", {})),
                    "scores": record["scores"][suite],
                    "no_gpu": suite in record.get("no_gpu", []),
                })
    return {suite: list(series.values()) for suite, series in charts.items()}


def budget_series(records: list[dict]) -> dict[str, list[dict]]:
    """For each budget measurement, one series per machine, each point beside its ceiling."""
    charts: dict[str, dict[str, dict]] = {}
    for record in records:
        if record.get("kind") != KIND_BUDGET:
            continue
        for name, measured in record["measurements"].items():
            series = charts.setdefault(name, {}).setdefault(
                record["machine"], {"machine": record["machine"], "points": []})
            series["points"].append({
                "date": record["date"],
                "value": measured["value"],
                "ceiling": measured["ceiling"],
                "engine": engine_label(record.get("engine", {})),
            })
    return {name: list(series.values()) for name, series in charts.items()}


# The page.

# Okabe and Ito's eight, which stay apart under the common colour-vision deficiencies. Each machine
# takes one in the order it first appears; the baselines are told apart by dash, not colour.
COLOURS = ["#0072B2", "#D55E00", "#009E73", "#CC79A7", "#E69F00", "#56B4E9", "#000000", "#F0E442"]
DASHES = {LATEST: "", MATCHED: "6 4"}

WIDTH, HEIGHT = 760, 260
LEFT, RIGHT, TOP, BOTTOM = 64, 16, 28, 40


def _timestamp(date: str) -> float:
    return datetime.datetime.fromisoformat(date.replace("Z", "+00:00")).timestamp()


class _Axes:
    """Maps dates and values onto one chart's box."""

    def __init__(self, dates: list[str], values: list[float]) -> None:
        stamps = [_timestamp(date) for date in dates]
        self.start, self.end = min(stamps), max(stamps)
        if self.end == self.start:
            self.start, self.end = self.start - 43200, self.end + 43200
        finite = [value for value in values if value == value]
        low, high = min(finite), max(finite)
        margin = (high - low) * 0.1 or abs(high) * 0.1 or 1.0
        self.low, self.high = low - margin, high + margin

    def x(self, date: str) -> float:
        share = (_timestamp(date) - self.start) / (self.end - self.start)
        return LEFT + share * (WIDTH - LEFT - RIGHT)

    def y(self, value: float) -> float:
        share = (value - self.low) / (self.high - self.low)
        return HEIGHT - BOTTOM - share * (HEIGHT - TOP - BOTTOM)

    def frame(self, unit: str) -> list[str]:
        parts = [f'<rect x="{LEFT}" y="{TOP}" width="{WIDTH - LEFT - RIGHT}" '
                 f'height="{HEIGHT - TOP - BOTTOM}" fill="none" stroke="#999"/>']
        for step in range(5):
            value = self.low + (self.high - self.low) * step / 4
            y = self.y(value)
            parts.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{y:.1f}" y2="{y:.1f}" '
                         'stroke="#ddd"/>')
            parts.append(f'<text x="{LEFT - 6}" y="{y + 4:.1f}" text-anchor="end">'
                         f'{value:.4g}</text>')
        for step in range(5):
            stamp = self.start + (self.end - self.start) * step / 4
            day = datetime.datetime.fromtimestamp(stamp, datetime.timezone.utc).date().isoformat()
            x = LEFT + (WIDTH - LEFT - RIGHT) * step / 4
            # The two ends are anchored inside the box, so neither date runs off the chart.
            anchor = {0: "start", 4: "end"}.get(step, "middle")
            parts.append(f'<text x="{x:.1f}" y="{HEIGHT - BOTTOM + 16}" text-anchor="{anchor}">'
                         f'{day}</text>')
        parts.append(f'<text x="{LEFT}" y="{TOP - 10}" class="unit">{html.escape(unit)}</text>')
        return parts

    def marks(self, updates: list[dict]) -> list[str]:
        parts = []
        for mark in updates:
            if not self.start <= _timestamp(mark["date"]) <= self.end:
                continue
            x = self.x(mark["date"])
            label = html.escape(f"{mark['engine']} on {mark['machine']}")
            parts.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{TOP}" y2="{HEIGHT - BOTTOM}" '
                         f'stroke="#888" stroke-dasharray="2 3"><title>engine update: {label}'
                         '</title></line>')
            # A mark in the right half is labelled to its left, so the label stays on the chart.
            left = x > (LEFT + WIDTH - RIGHT) / 2
            parts.append(f'<text x="{x - 3 if left else x + 3:.1f}" y="{TOP + 12}" class="mark" '
                         f'text-anchor="{"end" if left else "start"}">'
                         f'{html.escape(mark["engine"])}</text>')
        return parts


def _polyline(axes: _Axes, points: list[dict], colour: str, dash: str) -> str:
    coordinates = " ".join(f"{axes.x(point['date']):.1f},{axes.y(point['value']):.1f}"
                           for point in points)
    dashed = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<polyline points="{coordinates}" fill="none" stroke="{colour}" '
            f'stroke-width="1.5"{dashed}/>')


def _point(axes: _Axes, point: dict, colour: str, hollow: bool, title: str) -> str:
    fill = "#fff" if hollow else colour
    return (f'<circle cx="{axes.x(point["date"]):.1f}" cy="{axes.y(point["value"]):.1f}" r="4" '
            f'fill="{fill}" stroke="{colour}" stroke-width="1.5">'
            f'<title>{html.escape(title)}</title></circle>')


def _legend(entries: list[tuple[str, str, str]]) -> str:
    items = []
    for colour, dash, label in entries:
        dashed = f' stroke-dasharray="{dash}"' if dash else ""
        items.append(f'<span><svg width="28" height="10" aria-hidden="true"><line x1="0" x2="28" '
                     f'y1="5" y2="5" stroke="{colour}" stroke-width="2"{dashed}/></svg> '
                     f'{html.escape(label)}</span>')
    return f'<p class="legend">{" ".join(items)}</p>'


def _colours(records: list[dict]) -> dict[str, str]:
    machines: dict[str, str] = {}
    for record in records:
        machine = record.get("machine", "")
        if machine not in machines:
            machines[machine] = COLOURS[len(machines) % len(COLOURS)]
    return machines


def _svg(parts: list[str], label: str) -> str:
    return (f'<svg class="chart" viewBox="0 0 {WIDTH} {HEIGHT}" role="img" '
            f'aria-label="{html.escape(label)}">'
            + "".join(parts) + "</svg>")


def render(records: list[dict], source: str = "performance/history.jsonl") -> str:
    """The history as one page: the ratios per suite, then each budget measurement."""
    colours = _colours(records)
    updates = engine_updates(records)
    sections = []

    comparisons = comparison_series(records)
    if comparisons:
        sections.append("<h2>Omaweb against Chromium</h2><p>Omaweb's score over Chromium's, the "
                        "mean of each browser's runs. Under 1 is Omaweb behind. Solid is the "
                        "latest stable Chromium, dashed the Chromium the engine is based on. A "
                        "hollow point is a MotionMark run taken without GPU compositing, which "
                        "is not a result. Hover a point for its scores.</p>")
    for suite, series in comparisons.items():
        points = [point for line in series for point in line["points"]]
        axes = _Axes([point["date"] for point in points],
                     [point["value"] for point in points] + [1.0])
        parts = axes.frame("ratio")
        parts.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{axes.y(1.0):.1f}" '
                     f'y2="{axes.y(1.0):.1f}" stroke="#444"/>')
        parts += axes.marks(updates)
        legend = []
        for line in series:
            colour, dash = colours[line["machine"]], DASHES[line["baseline"]]
            parts.append(_polyline(axes, line["points"], colour, dash))
            for point in line["points"]:
                title = (f"{point['date']}  {line['machine']}\n"
                         f"{point['value']:.3f} against Chromium {point['chromium']} "
                         f"({BASELINE_NAMES[line['baseline']]})\n{point['engine']}\n"
                         f"scores: {_scores_text(point['scores'])}"
                         + ("\nno GPU compositing: not a result" if point["no_gpu"] else ""))
                parts.append(_point(axes, point, colour, point["no_gpu"], title))
            legend.append((colour, dash, f"{line['machine']}, {BASELINE_NAMES[line['baseline']]}"))
        sections.append(f"<h3>{html.escape(suite)}</h3>" + _svg(parts, f"{suite} ratio")
                        + _legend(legend))

    budgets = budget_series(records)
    if budgets:
        sections.append("<h2>Runtime budget</h2><p>Each measurement against its ceiling, which is "
                        "the thin line of the same colour. Hover a point for its value.</p>")
    for name, series in budgets.items():
        points = [point for line in series for point in line["points"]]
        axes = _Axes([point["date"] for point in points],
                     [point["value"] for point in points] + [point["ceiling"] for point in points])
        parts = axes.frame(name.rsplit("_", 1)[-1])
        parts += axes.marks(updates)
        legend = []
        for line in series:
            colour = colours[line["machine"]]
            ceiling = [{"date": point["date"], "value": point["ceiling"]} for point in line["points"]]
            parts.append(_polyline(axes, ceiling, colour, "1 3").replace(
                'stroke-width="1.5"', 'stroke-width="1"'))
            parts.append(_polyline(axes, line["points"], colour, ""))
            for point in line["points"]:
                title = (f"{point['date']}  {line['machine']}\n{point['value']:g} against a "
                         f"ceiling of {point['ceiling']:g}\n{point['engine']}")
                parts.append(_point(axes, point, colour, False, title))
            legend.append((colour, "", line["machine"]))
        sections.append(f"<h3>{html.escape(name)}</h3>" + _svg(parts, name) + _legend(legend))

    if not sections:
        sections.append("<p>The history has no runs yet.</p>")
    return f"""<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Omaweb performance history</title>
<style>
  body {{ font: 14px/1.4 sans-serif; margin: 16px auto; max-width: 800px; padding: 0 16px;
         color: #222; background: #fff; }}
  svg.chart {{ width: 100%; height: auto; font-size: 11px; }}
  .unit, .mark {{ fill: #666; }}
  .legend span {{ margin-right: 16px; white-space: nowrap; }}
</style>
<h1>Omaweb performance history</h1>
<p>{len(records)} runs from {html.escape(source)}, drawn {now()}. Dotted vertical lines are
engine updates.</p>
{"".join(sections)}
</html>
"""
