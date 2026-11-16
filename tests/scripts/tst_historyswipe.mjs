import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { runInNewContext } from "node:vm";
import { test } from "node:test";

const source = readFileSync(
  new URL("../../src/engine/qt/history-swipe.js", import.meta.url),
  "utf8",
);

function fixture() {
  const element = (options = {}) => {
    const value = {
      tagName: "DIV",
      localName: "div",
      scrollWidth: 500,
      clientWidth: 500,
      scrollLeft: 0,
      parentElement: null,
      style: { overflowX: "visible", overscrollBehaviorX: "auto", direction: "ltr" },
      getRootNode: () => ({ host: null }),
      ...options,
    };
    return value;
  };
  const root = element();
  let target = root;
  const document = {
    scrollingElement: root,
    documentElement: root,
    elementFromPoint: () => target,
  };
  const context = { document, getComputedStyle: (node) => node.style };
  runInNewContext(source, context);
  return {
    element,
    root,
    document,
    over: (node) => (target = node),
    allowed: (direction) => context.__omawebHistorySwipeCanStart(40, 40, direction),
  };
}

test("a page without horizontal scrolling permits either history direction", () => {
  const f = fixture();
  assert.equal(f.allowed(-1), true);
  assert.equal(f.allowed(1), true);
});

test("a wide page permits navigation only at the corresponding edge", () => {
  const f = fixture();
  f.root.scrollWidth = 1000;
  assert.equal(f.allowed(-1), true);
  assert.equal(f.allowed(1), false);
  f.root.scrollLeft = 250;
  assert.equal(f.allowed(-1), false);
  assert.equal(f.allowed(1), false);
  f.root.scrollLeft = 500;
  assert.equal(f.allowed(1), true);
});

test("a nested horizontal scroller owns a gesture until its edge", () => {
  const f = fixture();
  const nested = f.element({ scrollWidth: 1000, scrollLeft: 100, parentElement: f.root });
  nested.style.overflowX = "auto";
  const child = f.element({ parentElement: nested });
  f.over(child);
  assert.equal(f.allowed(-1), false);
  assert.equal(f.allowed(1), false);
  nested.scrollLeft = 0;
  assert.equal(f.allowed(-1), true);
});

test("RTL uses Chromium's negative scrollLeft range", () => {
  const f = fixture();
  f.root.style.direction = "rtl";
  f.root.scrollWidth = 1000;
  assert.equal(f.allowed(-1), false);
  assert.equal(f.allowed(1), true);
  f.root.scrollLeft = -250;
  assert.equal(f.allowed(-1), false);
  assert.equal(f.allowed(1), false);
  f.root.scrollLeft = -500;
  assert.equal(f.allowed(-1), true);
});

test("overscroll containment prevents history navigation", () => {
  for (const policy of ["contain", "none"]) {
    const f = fixture();
    f.root.style.overscrollBehaviorX = policy;
    assert.equal(f.allowed(-1), false);
    assert.equal(f.allowed(1), false);
  }
});

test("frames, canvas, native inputs and opaque components keep their gestures", () => {
  for (const tag of [
    "IFRAME",
    "FRAME",
    "EMBED",
    "OBJECT",
    "CANVAS",
    "INPUT",
    "TEXTAREA",
    "X-PANE",
  ]) {
    const f = fixture();
    f.over(f.element({ tagName: tag, localName: tag.toLowerCase(), parentElement: f.root }));
    assert.equal(f.allowed(-1), false, tag);
    assert.equal(f.allowed(1), false, tag);
  }
});

test("open shadow roots expose their own scroll chain", () => {
  const f = fixture();
  const host = f.element({ localName: "x-pane", parentElement: f.root });
  const inner = f.element({ scrollWidth: 1000, scrollLeft: 100 });
  inner.style.overflowX = "auto";
  inner.getRootNode = () => ({ host });
  host.shadowRoot = { elementFromPoint: () => inner };
  f.over(host);
  assert.equal(f.allowed(-1), false);
  inner.scrollLeft = 0;
  assert.equal(f.allowed(-1), true);
});

test("missing targets and fullscreen pages fail closed", () => {
  const f = fixture();
  f.document.fullscreenElement = f.root;
  assert.equal(f.allowed(-1), false);
  f.document.fullscreenElement = null;
  f.over(null);
  assert.equal(f.allowed(-1), false);
});
