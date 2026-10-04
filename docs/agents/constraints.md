# Constraints

Settled by the maintainer. A proposal that needs one of these broken goes to the maintainer as a
question, with the reason, before any work.

- **The interface stays QML**, so it can follow Omarchy's own QML components. Engine and
  architecture options that keep a QML interface are in scope: QtWebEngine, CEF, a newer Qt, patches
  to the engine's drawing path. A Chromium fork with a Views or WebUI interface, or Electron, is
  out.
- **GPU compositing stays on.** `--disable-gpu-compositing` is a diagnostic that shows which layer
  fails, never a fix, by default or for some machines. Omaweb is measured against Chromium (#436),
  and rendering on the CPU gives that away.
