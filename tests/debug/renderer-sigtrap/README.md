# Renderer SIGTRAP on the first page

Debug tools for [#442](https://github.com/villekivela/omaweb/issues/442): on CI, about one page-load
run in 700 loses load 0 to a renderer that stops itself with SIGTRAP. Nothing here runs in CI or in
`ctest`.

## What is here

- `pageload.patch` turns a copy of `scripts/benchmark_runtime.py` into the loop's run: two loads
  instead of the whole plan (the crash has only been seen on load 0), the browser's stderr kept in
  `LOOP_KEEP_LOG`, and `/dev/dri` covered in the private network when `LOOP_NO_GPU` is set.
- `host/start.sh` runs the loop on this machine, in headless cages, against a browser built from any
  tree. Read the usage at its top. It unsets the reader's compositor and sets
  `OMAWEB_NO_OMARCHY_TEMPLATE`, so the reader's desktop and theme are left alone.
- `container/run.sh` runs the same loop in a throwaway `archlinux:base-devel` container, shaped like
  CI's `arch-linux-budget` job: Omaweb's engine from its repository, dnsmasq as root on the
  container's resolver, and `--shm-size=2g`.
- `trapreport.c` is a preload shim that writes a SIGTRAP's stack to stderr as `module+offset`
  frames, then dies of the same signal, for a runner that keeps no core and allows no ptrace.
  `container/setup.sh` builds it as `/work/trapreport.so`; run the container with
  `LOOP_EXTRA=LD_PRELOAD=/work/trapreport.so`. It is not validated: a renderer sent SIGTRAP by hand
  dumped core but printed no frames, so check first whether the zygote passes `LD_PRELOAD` to its
  renderers.

A run that went quiet or printed a crash line is kept as `keep-<n>-run.txt` and
`keep-<n>-stderr.txt` beside the worker's output. A crash on a machine that keeps cores lands in
`coredumpctl`, and engine-686's 6.11.2-14 tree has the symbols for the container's engine.

## What was run

On 2026-10-11, on an x86_64 Ryzen 7 PRO 7840HS, about 6,300 runs, none of which crashed. The first
rows ran the twelve loads of one measured load per case, before the patch cut the plan to two.

| Where                              | Engine                        | Rendering                    | Runs  |
| ---------------------------------- | ----------------------------- | ---------------------------- | ----- |
| Host, twelve loads                 | Arch's qt6-webengine 6.11.2-1 | the laptop's GPU             | 1,398 |
| Host, twelve loads, `/dev/dri` off | omaweb-qtwebengine 6.11.2-7   | EGL fails as on CI, llvmpipe | 1,369 |
| Container, twelve loads, 8 CPUs    | omaweb-qtwebengine 6.11.2-14  | EGL fails as on CI           | 765   |
| Container, two loads, 8 CPUs       | omaweb-qtwebengine 6.11.2-14  | EGL fails as on CI           | 2,605 |
| Container, two loads, 4 CPUs       | omaweb-qtwebengine 6.11.2-14  | `QT_QUICK_BACKEND=software`  | 204   |

The last row is the only one whose renderers ran with `--disable-gpu-compositing`. Whether CI's do
is not known, and it is the first thing to settle.

## What CI showed

In run 36389929217 of the `scratch/394-experiments` diagnostics, shard 15 lost load 0 twice in a
minute. Both times the renderer ended 128 ms after the navigation to `/page/0` started, after all 40
images were asked for and answered, with termination status 3 and exit code 5. The browser's stderr
held Mesa's EGL and Vulkan failures and no `Check failed` or stack line, so it was a silent
`IMMEDIATE_CRASH()`.
