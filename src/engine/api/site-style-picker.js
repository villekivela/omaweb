// The Site style panel's picker, run in a world of the engine's own beside the
// page's, so the page can neither see it nor call it. Hovering highlights an
// element, a click picks it, and the keys move the pick: Up and Down to the
// parent and back to the child, Left and Right to the siblings, [ and ] to
// narrow and widen, Enter to hide and Escape to stop. What is picked is
// reported as a selector and how many elements it matches; the shell decides
// what to write.
//
// `report(channel, payload)` is the engine's sealed report, declared before
// this source runs. Each report is one of:
//   site_style_pick  { selector, count, level, levels, valid }
//   site_style_hide  { selector }
//   site_style_stop  {}
(() => {
  const previous = globalThis.__omawebSiteStylePicker;
  if (previous) previous.stop(false);

  // A class or id a site's build tool made up says nothing about the element
  // and changes with the next build, so a selector does not lean on one.
  const generated =
    /\d{3,}|^(css|sc|jsx|svelte|emotion|styled|tw)-|^_|[_-][a-z0-9]{5,}$|[A-Z].*\d|:|\[/i;
  const states = /^(is-|has-)?(active|hover|focus|focused|selected|open|visible|hidden)$/i;
  const stableClass = (name) =>
    name.length > 0 && name.length <= 32 && !generated.test(name) && !states.test(name);
  const stableId = (id) => /^[A-Za-z][\w-]*$/.test(id) && id.length <= 48 && !/\d{4,}/.test(id);
  const attributes = ["data-testid", "data-test", "role", "aria-label", "name"];

  const count = (selector) => {
    try {
      return document.querySelectorAll(selector).length;
    } catch (error) {
      return -1;
    }
  };
  const unique = (selector, element) => {
    try {
      const found = document.querySelectorAll(selector);
      return found.length === 1 && found[0] === element;
    } catch (error) {
      return false;
    }
  };

  // One element as a compound selector: its tag and the classes that are
  // its own, or an attribute that names what it is where it has no class.
  const step = (element) => {
    const tag = element.localName;
    const classes = Array.from(element.classList).filter(stableClass).slice(0, 3);
    if (classes.length > 0) return tag + classes.map((name) => "." + CSS.escape(name)).join("");
    for (const name of attributes) {
      const value = element.getAttribute(name);
      if (value && value.length <= 48) return tag + "[" + name + '="' + CSS.escape(value) + '"]';
    }
    return tag;
  };
  const position = (element) => {
    const parent = element.parentElement;
    return parent ? Array.prototype.indexOf.call(parent.children, element) + 1 : 1;
  };

  // The selectors an element can be hidden by, from it alone to everything
  // related, each matching more than the one before. The first is unique: an
  // id if it has a stable one, else the steps up to the nearest ancestor with
  // one, and `:nth-child` only where nothing else tells it from its siblings.
  const candidates = (element) => {
    const found = [];
    const add = (selector) => {
      const matched = count(selector);
      if (matched > 0 && (found.length === 0 || matched > found[found.length - 1].count))
        found.push({ selector, count: matched });
    };
    if (element.id && stableId(element.id) && unique("#" + CSS.escape(element.id), element)) {
      add("#" + CSS.escape(element.id));
    } else {
      const steps = [];
      let anchor = "";
      let exact = "";
      for (let at = element; at && at !== document.documentElement; at = at.parentElement) {
        if (at !== element && at.id && stableId(at.id) && unique("#" + CSS.escape(at.id), at)) {
          anchor = "#" + CSS.escape(at.id);
          break;
        }
        steps.unshift(step(at));
        const tried = steps.join(" > ");
        if (unique(tried, element)) {
          exact = tried;
          break;
        }
      }
      if (!exact && anchor && unique(anchor + " > " + steps.join(" > "), element))
        exact = anchor + " > " + steps.join(" > ");
      if (!exact) {
        // The last resort: each step pinned to its place among its siblings,
        // from the element up, until one is enough.
        const path = [];
        for (let at = element; at && at !== document.documentElement; at = at.parentElement)
          path.unshift(at);
        const pinned = path.map(step);
        for (let index = pinned.length - 1; index >= 0 && !exact; --index) {
          pinned[index] += ":nth-child(" + position(path[index]) + ")";
          const tried = (anchor ? anchor + " > " : "") + pinned.slice(-steps.length).join(" > ");
          if (unique(tried, element)) exact = tried;
        }
        if (!exact) exact = pinned.join(" > ");
      }
      add(exact);
      if (anchor) add(anchor + " " + step(element));
    }
    const own = step(element);
    const parent = element.parentElement;
    if (parent && parent !== document.body && parent !== document.documentElement)
      add(step(parent) + " > " + own);
    add(own);
    return found;
  };

  // ---- the highlight ----------------------------------------------------
  //
  // Drawn in a closed shadow root over the page, taking no pointer, so the
  // page's own styles cannot reach it and it never stands in the way of what
  // it points at.
  const host = document.createElement("div");
  host.style.cssText = "all:initial;position:fixed;inset:0;pointer-events:none;z-index:2147483647";
  const shadow = host.attachShadow({ mode: "closed" });
  const accent = globalThis.__omawebSiteStyleAccent || "#7c6cff";
  shadow.innerHTML =
    "<style>.box{position:fixed;box-sizing:border-box;border:2px solid " +
    accent +
    ";border-radius:4px;background:color-mix(in srgb," +
    accent +
    " 18%,transparent)}.box.related{opacity:.6}" +
    ".label{position:fixed;font:12px/1.4 monospace;padding:2px 8px;border-radius:4px;" +
    "color:#fff;background:" +
    accent +
    ";white-space:nowrap}</style>";
  const layer = document.createElement("div");
  shadow.appendChild(layer);

  const state = { element: null, locked: false, trail: [], level: 0, typed: "" };

  const draw = (selector) => {
    layer.textContent = "";
    let matched = [];
    try {
      matched = selector ? Array.from(document.querySelectorAll(selector)).slice(0, 200) : [];
    } catch (error) {
      matched = [];
    }
    for (const element of matched) {
      const rect = element.getBoundingClientRect();
      const box = document.createElement("div");
      box.className = element === state.element ? "box" : "box related";
      box.style.cssText = `left:${rect.left}px;top:${rect.top}px;width:${rect.width}px;height:${rect.height}px`;
      layer.appendChild(box);
    }
    const shown = state.element || matched[0];
    if (shown) {
      const rect = shown.getBoundingClientRect();
      const label = document.createElement("div");
      label.className = "label";
      label.textContent =
        selector +
        "  " +
        Math.round(rect.width) +
        " × " +
        Math.round(rect.height) +
        (matched.length > 1 ? "  · " + matched.length : "");
      label.style.cssText = `left:${Math.max(0, rect.left)}px;top:${Math.max(0, rect.top + 6)}px`;
      layer.appendChild(label);
    }
  };

  const current = () => {
    if (state.typed) {
      const matched = count(state.typed);
      return {
        selector: state.typed,
        count: Math.max(0, matched),
        level: 0,
        levels: 1,
        valid: matched >= 0,
      };
    }
    if (!state.element) return { selector: "", count: 0, level: 0, levels: 0, valid: false };
    const found = candidates(state.element);
    const level = Math.max(0, Math.min(state.level, found.length - 1));
    state.level = level;
    const chosen = found[level] || { selector: "", count: 0 };
    return {
      selector: chosen.selector,
      count: chosen.count,
      level,
      levels: found.length,
      valid: true,
    };
  };

  const announce = () => {
    const now = current();
    draw(now.valid ? now.selector : "");
    report("site_style_pick", now);
  };

  const choose = (element, locked) => {
    if (!element || element === host || element === document.documentElement) return;
    if (element !== state.element) state.level = 0;
    state.element = element;
    state.locked = locked;
    state.typed = "";
    announce();
  };

  const pointed = (event) => {
    const element = document.elementFromPoint(event.clientX, event.clientY);
    return element && element !== host ? element : null;
  };

  const withheld = (event) => {
    event.preventDefault();
    event.stopImmediatePropagation();
  };

  const listeners = {
    mousemove(event) {
      if (!state.locked) choose(pointed(event), false);
    },
    pointerdown: withheld,
    mousedown: withheld,
    pointerup: withheld,
    mouseup: withheld,
    dblclick: withheld,
    click(event) {
      withheld(event);
      state.trail = [];
      choose(pointed(event), true);
    },
    keydown(event) {
      const moves = {
        ArrowUp: "parent",
        ArrowDown: "child",
        ArrowLeft: "previous",
        ArrowRight: "next",
      };
      if (event.altKey || event.ctrlKey || event.metaKey) return;
      if (moves[event.key]) picker.move(moves[event.key]);
      else if (event.key === "[") picker.widen(state.level - 1);
      else if (event.key === "]") picker.widen(state.level + 1);
      else if (event.key === "Enter") picker.hide();
      else if (event.key === "Escape") picker.stop(true);
      else return;
      withheld(event);
    },
    scroll() {
      announceDrawn();
    },
    resize() {
      announceDrawn();
    },
  };
  const announceDrawn = () => {
    const now = current();
    draw(now.valid ? now.selector : "");
  };

  const picker = {
    move(direction) {
      const element = state.element;
      if (!element) return;
      let next = null;
      if (direction === "parent") {
        next = element.parentElement;
        if (next === document.documentElement) next = null;
        if (next) state.trail.push(element);
      } else if (direction === "child") {
        next = state.trail.pop() || element.firstElementChild;
      } else if (direction === "previous") {
        next = element.previousElementSibling;
        state.trail = [];
      } else if (direction === "next") {
        next = element.nextElementSibling;
        state.trail = [];
      }
      if (next) choose(next, true);
    },
    widen(level) {
      state.typed = "";
      state.level = Math.max(0, level);
      announce();
    },
    select(text) {
      state.typed = String(text).trim();
      announce();
    },
    hide() {
      const now = current();
      if (!now.valid || now.count === 0) return;
      report("site_style_hide", { selector: now.selector });
      picker.stop(false);
    },
    stop(say) {
      for (const name in listeners) removeEventListener(name, listeners[name], true);
      host.remove();
      if (globalThis.__omawebSiteStylePicker === picker) delete globalThis.__omawebSiteStylePicker;
      if (say) report("site_style_stop", {});
    },
  };

  for (const name in listeners) addEventListener(name, listeners[name], true);
  document.documentElement.appendChild(host);
  globalThis.__omawebSiteStylePicker = picker;
  report("site_style_pick", current());
})();
