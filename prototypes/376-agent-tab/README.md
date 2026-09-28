# PROTOTYPE: keep an Agent tab rendered and give it trusted input (#376)

Throwaway. It answers the spike in #376 for ADR 0051 and never merges. It is a standalone Qt Quick
program, not Omaweb: one window holds two `WebEngineView`s filling the same page area, the reader's
page on top and the Agent tab behind it, each on its own off-the-record profile so each has its own
renderer. A scenario in `main.qml` runs the four questions and prints one `RESULT` line of JSON per
measurement. `probe.cpp` sends Qt events to the view's `RenderWidgetHostViewQtDelegateItem`, reads
the CPU the process tree spent, times the window's frames and reads pixels.

The Agent tab is put out of sight in one of four ways:

- `frozen`: `visible: false` and `LifecycleState.Frozen`, which is an away Space's page today.
- `hidden`: `visible: false` only.
- `behind`: visible, same size, stacked under the reader's view. The spike's proposal.
- `opacity0`: visible, stacked under, `opacity: 0`, so the scene graph does not draw it.

`front` puts the Agent tab on show, as the price of the same page when the reader is looking at it.

## Run it

```sh
# macOS, Homebrew Qt. QSG_RHI_PROFILE=1 turns on the GPU timestamps.
cmake -S prototypes/376-agent-tab -B /tmp/probe -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build /tmp/probe
QSG_RHI_PROFILE=1 /tmp/probe/omaweb-376-probe            # all four parts
/tmp/probe/omaweb-376-probe rendering grab input         # or some of them

# Arch against Omaweb's engine, inside a Wayland session.
cmake -S prototypes/376-agent-tab -B /tmp/probe -G Ninja \
    -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=/usr/lib/omaweb
cmake --build /tmp/probe
QML_IMPORT_PATH=/usr/lib/omaweb/lib/qt6/qml \
QTWEBENGINEPROCESS_PATH=/usr/lib/omaweb/lib/qt6/QtWebEngineProcess \
QTWEBENGINE_RESOURCES_PATH=/usr/lib/omaweb/share/qt6/resources \
QTWEBENGINE_LOCALES_PATH=/usr/lib/omaweb/share/qt6/translations/qtwebengine_locales \
    /tmp/probe/omaweb-376-probe
```

`PROBE_ROUNDS` sets how many interleaved rounds the cost part takes (3), and `PROBE_OUT` where the
grabs are saved. Keep the window uncovered while it runs: a covered window stops drawing, and every
renderer then reads zero.

## Findings

[`FINDINGS.md`](FINDINGS.md) answers the four questions, and `results/` holds the raw `RESULT` lines
from the Linux VM and the macOS dev build it was measured on.
