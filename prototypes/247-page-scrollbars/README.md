# Prototype: an Omaweb-drawn scrollbar for webpages

Throwaway. Built to answer #247. Not wired into the browser and not meant to be.

## The question

Can Omaweb hide the engine's scrollbar and draw its own, shown only on hover? The doubt was
transport: the page's scroll position has to cross a process boundary before the bar can be drawn,
and a bar that trails the page is worse than the engine's own.

## Answer: yes, and the transport is not the problem

Scroll position reaches QML in **1-2 ms**, measured on a long article. A frame is 16.7 ms, so the
bar has the position well before it needs to draw. Latency was never the risk it looked like.

No new plumbing is needed either. `EngineView.qml` already dispatches page-to-chrome messages in
`onJavaScriptConsoleMessage` on an `__omaweb_*` prefix, which is what this uses.

## What it does

- Hides **only** the viewport's scrollbar, with `:where(html){scrollbar-width:none}`. Every inner
  scroller keeps its own: confirmed against Wikipedia, whose Contents sidebar still has its bar.
  This is the whole reason the approach is tractable. `--blink-settings=hideScrollbars=true` hides
  every scrollbar in the document, which would mean redrawing bars for every `overflow:auto` box on
  the page.
- Reports `scrollY`, `scrollHeight` and `innerHeight` on scroll and resize, plus a 500 ms heartbeat.
- Draws the bar in QML over the view, gated on the pointer being in a 14px gutter at the edge.
- Drags scroll the page through `window.scrollTo`.

## Running it

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/scrollproto        # hover the right edge to raise the bar
./build/scrollproto show   # pins the bar visible
```

The overlay in the bottom-left prints scroll position, update count, last latency, worst gap between
updates, and the hover and drag state.

`show` exists because a warped cursor produces no Wayland pointer-enter, so the hover gate cannot be
driven from a script. Hover and drag were the two things left for a person to judge.

## What this does not answer

- How it feels to use. That needs hands on it.
- Wheel-scrolling while the pointer sits in the gutter, the one case where the bar is visible and
  Omaweb is not the source of the scroll position.
- Iframes, which are their own documents with their own scrollers.
- Pages that set `scrollbar-width` themselves. `:where()` has no specificity, so such a page wins
  and keeps its own bar, which is the intended outcome but is untested here.
- Whether the gutter swallowing pointer events in its 14px strip is acceptable over a webpage.
