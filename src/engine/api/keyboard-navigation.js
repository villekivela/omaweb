(() => {
  if (globalThis.__omawebKeyboardNavigation) return;
  const state = {
    config: { enabled: false, bindings: {}, passthroughAll: false, passthroughKeys: [] },
    prefix: "",
    prefixTimer: 0,
    hintInput: "",
    hints: [],
    hintMode: "",
    hintFocus: null,
    previousFocus: null,
  };
  const hintAlphabet = "sadfjklewcmpgh";
  const keyName = (event) =>
    event.shiftKey && event.key.toLowerCase() === "f" ? "Shift+F" : event.key;
  const editable = (target) =>
    target &&
    (target.isContentEditable ||
      (typeof target.closest === "function" &&
        target.closest('input, textarea, select, [contenteditable="true"]')));
  // A hinted field wants the keyboard, not a click: a synthetic click never
  // moves focus, so the reader would land on a field they cannot type in.
  const typeableInput = new Set([
    "text",
    "search",
    "url",
    "email",
    "tel",
    "password",
    "number",
    "date",
    "datetime-local",
    "month",
    "week",
    "time",
  ]);
  const field = (target) => {
    if (target.tagName === "TEXTAREA" || target.tagName === "SELECT") return true;
    if (target.tagName === "INPUT") {
      return typeableInput.has(String(target.type || "text").toLowerCase());
    }
    return target.isContentEditable === true;
  };
  const visible = (element) => {
    const style = getComputedStyle(element);
    const rect = element.getBoundingClientRect();
    return (
      style.visibility !== "hidden" &&
      style.display !== "none" &&
      rect.width > 0 &&
      rect.height > 0 &&
      rect.bottom >= 0 &&
      rect.right >= 0 &&
      rect.top <= innerHeight &&
      rect.left <= innerWidth
    );
  };
  // Breadth-first allocation permits short and long hints on the same page
  // without making a complete hint the prefix of another one.
  const labelsFor = (count) => {
    let labels = [""];
    let offset = 0;
    while (labels.length - offset < count || labels.length === 1) {
      const suffix = labels[offset++];
      for (const character of hintAlphabet) labels.push(character + suffix);
    }
    return labels
      .slice(offset, offset + count)
      .sort()
      .map((label) => [...label].reverse().join(""));
  };
  const clearHints = () => {
    const wasActive = Boolean(state.hintMode);
    document.getElementById("__omaweb_link_hints")?.remove();
    if (state.previousFocus?.isConnected) state.previousFocus.focus({ preventScroll: true });
    state.hints = [];
    state.hintInput = "";
    state.hintMode = "";
    state.hintFocus = null;
    state.previousFocus = null;
    if (wasActive) console.debug("__omaweb_keyboard_hint_mode__:0");
  };
  const clearPrefix = () => {
    state.prefix = "";
    clearTimeout(state.prefixTimer);
    document.getElementById("__omaweb_key_sequence")?.remove();
  };
  const showPrefix = (key) => {
    document.getElementById("__omaweb_key_sequence")?.remove();
    const indicator = document.createElement("span");
    indicator.id = "__omaweb_key_sequence";
    indicator.textContent = key + "…";
    indicator.setAttribute("role", "status");
    indicator.setAttribute("aria-label", "Keyboard sequence " + key);
    indicator.style.cssText =
      "position:fixed;right:1rem;bottom:1rem;z-index:2147483647;" +
      "padding:.25em .5em;border:2px solid ButtonText;border-radius:.25em;" +
      "background:ButtonFace;color:ButtonText;font:700 max(12px,.8rem) system-ui,sans-serif;";
    document.documentElement.append(indicator);
  };
  const showHints = (command) => {
    clearHints();
    let candidates = [
      ...document.querySelectorAll(
        'a[href], button, input:not([type="hidden"]), select, textarea, [role="link"], [role="button"], [tabindex]',
      ),
    ].filter(visible);
    if (command === "open-link-background") {
      candidates = candidates.filter((target) => Boolean(target.href));
    }
    const overlay = document.createElement("div");
    overlay.id = "__omaweb_link_hints";
    overlay.setAttribute("role", "status");
    overlay.setAttribute("aria-live", "polite");
    overlay.setAttribute("aria-label", candidates.length + " link hints available");
    const theme = state.config.hintTheme || {};
    const font = theme.font || {};
    const fontFamily = font.family || (font.families && font.families[0]) || "system-ui";
    const fontSize = Math.max(11, Number(font.size) || 12);
    overlay.style.cssText =
      "position:fixed;inset:0;pointer-events:none;z-index:2147483647;" +
      "color:" +
      (theme.text || "CanvasText") +
      ";font-family:" +
      JSON.stringify(fontFamily) +
      ",monospace;font-size:" +
      fontSize +
      "px;font-weight:600;";
    const focus = document.createElement("input");
    focus.id = "__omaweb_link_hint_input";
    focus.type = "text";
    focus.tabIndex = -1;
    focus.setAttribute("aria-label", "Type a link hint");
    focus.setAttribute("autocomplete", "off");
    focus.style.cssText =
      "position:fixed;left:-2px;top:-2px;width:1px;height:1px;" +
      "opacity:.001;pointer-events:none;";
    overlay.append(focus);
    const labels = labelsFor(candidates.length);
    state.hints = candidates.map((target, index) => {
      const label = labels[index];
      const rect = target.getBoundingClientRect();
      const hint = document.createElement("span");
      hint.setAttribute("role", "note");
      hint.setAttribute(
        "aria-label",
        "Link hint " +
          label +
          " for " +
          (
            target.getAttribute("aria-label") ||
            target.textContent ||
            target.href ||
            "target"
          ).trim(),
      );
      // The chip is the sidebar's two-letter site chip, moved onto the
      // page: radius 2, the code in the accent on a plain surface plate,
      // and no drop shadow. The border is what the sidebar can do
      // without, since a page may put anything behind the plate.
      hint.style.cssText =
        "position:absolute;left:" +
        Math.max(0, rect.left) +
        "px;top:" +
        Math.max(0, rect.top) +
        "px;padding:1px 4px;border:1px solid " +
        (theme.accent || "Highlight") +
        ";border-radius:2px;background:" +
        (theme.surface || "Canvas") +
        ";color:" +
        (theme.accent || "CanvasText") +
        ";line-height:1.2;forced-color-adjust:auto;";
      for (const character of label.toUpperCase()) {
        const part = document.createElement("span");
        part.textContent = character;
        hint.append(part);
      }
      overlay.append(hint);
      return { label, target, hint };
    });
    document.documentElement.append(overlay);
    state.hintMode = command;
    console.debug("__omaweb_keyboard_hint_mode__:1");
    state.previousFocus = document.activeElement;
    state.hintFocus = focus;
    focus.focus({ preventScroll: true });
  };
  const activateHint = (entry) => {
    const target = entry.target;
    const background = state.hintMode === "open-link-background";
    const takesFocus = !background && field(target);
    // Hint mode borrowed the focus and gives it back on the way out, which
    // would take it straight off the field again. The field keeps it.
    if (takesFocus) state.previousFocus = null;
    clearHints();
    if (takesFocus) {
      target.focus({ preventScroll: true });
    } else if (background) {
      target.dispatchEvent(
        new MouseEvent("click", {
          bubbles: true,
          cancelable: true,
          view: window,
          button: 0,
          ctrlKey: true,
          metaKey: true,
        }),
      );
    } else {
      target.click();
    }
  };
  // One glide, steered. Every press moves a single target and a frame loop
  // eases what is left of the distance toward it, so a held key reads as one
  // continuous run and a press lands on the next frame. The engine's own
  // smooth scroll cannot do either: each call restarts its ease from a stop,
  // and a jump to an end animates the document's whole length.
  const glide = { frame: 0, time: 0, position: 0, target: 0 };
  // What is left of the distance halves every 40 ms. The pace is the same at
  // any frame rate, and it holds when a frame arrives late.
  const glideHalfLife = 40;
  // Within a pixel of the target the run is over: the page cannot draw the
  // difference, and the scroller reports its position rounded anyway.
  const glideEpsilon = 0.5;
  // The scroller rounds what it is given to the device's pixels, so the
  // position it reports back is never exactly the position asked for. A wheel
  // or a drag moves the page by far more than this, which is what the loop is
  // watching for.
  const glideRounding = 2;
  const scroller = () => document.scrollingElement || document.documentElement;
  const viewport = () => scroller().clientHeight || innerHeight;
  const scrollLimit = () => Math.max(0, scroller().scrollHeight - viewport());
  const placeScroll = (offset) => scroller().scrollTo({ top: offset, behavior: "instant" });
  // The page is free to go elsewhere: the reader turns a wheel, an end is
  // reached, the document shrinks. Whatever the scroller says beats what the
  // run intended, and the run ends there rather than dragging the page back.
  // Idle the target is the position, so this also reads the page back in
  // before a press extends a run that is already over.
  const resyncGlide = () => {
    const standing = scroller().scrollTop;
    if (Math.abs(standing - glide.position) <= glideRounding) return;
    glide.position = standing;
    glide.target = standing;
  };
  const stepGlide = (now) => {
    resyncGlide();
    if (glide.position === glide.target) {
      glide.frame = 0;
      return;
    }
    // A frame's timestamp is when the frame began, which can predate the press
    // that started the run, and a late frame must not ease the whole distance
    // at once. Both ends of the step are held to a frame's worth of time.
    const elapsed = Math.min(64, Math.max(0, now - glide.time));
    glide.time = now;
    glide.position += (glide.target - glide.position) * (1 - Math.pow(2, -elapsed / glideHalfLife));
    if (Math.abs(glide.target - glide.position) < glideEpsilon) glide.position = glide.target;
    placeScroll(glide.position);
    glide.frame = glide.position === glide.target ? 0 : requestAnimationFrame(stepGlide);
  };
  const glideTo = (offset) => {
    resyncGlide();
    glide.target = Math.min(scrollLimit(), Math.max(0, offset));
    if (matchMedia("(prefers-reduced-motion: reduce)").matches) {
      cancelAnimationFrame(glide.frame);
      glide.frame = 0;
      glide.position = glide.target;
      placeScroll(glide.target);
      return;
    }
    // Anything past the last two screens of the distance is covered at once,
    // so an end lands at the same speed on a short page and on a long one.
    const reach = viewport() * 2;
    if (Math.abs(glide.target - glide.position) > reach) {
      glide.position = glide.target + (glide.target > glide.position ? -reach : reach);
      placeScroll(glide.position);
    }
    if (glide.frame || glide.position === glide.target) return;
    glide.time = performance.now();
    glide.frame = requestAnimationFrame(stepGlide);
  };
  // Measured from the target rather than from the page, so a held key adds to
  // the run in flight instead of restarting it a step ahead of where the page
  // happens to have reached.
  const glideBy = (distance) => {
    resyncGlide();
    glideTo(glide.target + distance);
  };
  const execute = (command) => {
    if (command === "scroll-down") glideBy(60);
    else if (command === "scroll-up") glideBy(-60);
    else if (command === "scroll-half-page-down") glideBy(viewport() * 0.5);
    else if (command === "scroll-half-page-up") glideBy(viewport() * -0.5);
    else if (command === "scroll-top") glideTo(0);
    else if (command === "scroll-bottom") glideTo(scrollLimit());
    else if (command === "open-link" || command === "open-link-background") showHints(command);
  };
  addEventListener(
    "keydown",
    (event) => {
      if (!state.config.enabled || event.defaultPrevented || event.isComposing) return;
      if (state.hintMode) {
        if (event.key === "Escape") {
          event.preventDefault();
          event.stopImmediatePropagation();
          clearHints();
          return;
        }
        if (event.key === "Backspace") {
          event.preventDefault();
          event.stopImmediatePropagation();
          state.hintInput = state.hintInput.slice(0, -1);
        } else {
          if (event.ctrlKey || event.metaKey || event.altKey || event.key.length !== 1) return;
          event.preventDefault();
          event.stopImmediatePropagation();
          state.hintInput += event.key.toLowerCase();
        }
        const matches = state.hints.filter((item) => item.label.startsWith(state.hintInput));
        state.hints.forEach((item) => {
          item.hint.style.display = matches.includes(item) ? "" : "none";
          Array.from(item.hint.children).forEach((part, index) => {
            part.style.color =
              index < state.hintInput.length ? state.config.hintTheme?.mutedText || "GrayText" : "";
          });
        });
        if (matches.length === 1 && matches[0].label === state.hintInput) activateHint(matches[0]);
        else if (!matches.length) clearHints();
        return;
      }
      const key = keyName(event);
      const passthrough = state.config.passthroughAll || state.config.passthroughKeys.includes(key);
      // A focused field swallows every binding, so the reader needs a way
      // out: Escape blurs it and hands the keyboard back to the page. A site
      // that asked for passthrough keeps Escape for itself.
      if (editable(event.target)) {
        if (event.key !== "Escape" || passthrough) return;
        if (event.ctrlKey || event.metaKey || event.altKey) return;
        const focused = document.activeElement;
        if (typeof focused?.blur !== "function") return;
        event.preventDefault();
        event.stopImmediatePropagation();
        focused.blur();
        return;
      }
      if (event.ctrlKey || event.metaKey || event.altKey || passthrough) return;
      const bindings = state.config.bindings || {};
      if (state.prefix) {
        const sequence = state.prefix + key;
        clearPrefix();
        if (bindings[sequence]) {
          event.preventDefault();
          event.stopImmediatePropagation();
          execute(bindings[sequence]);
          return;
        }
      }
      const hasSequence = Object.keys(bindings).some(
        (binding) => binding.length > key.length && binding.startsWith(key),
      );
      if (hasSequence) {
        event.preventDefault();
        event.stopImmediatePropagation();
        state.prefix = key;
        showPrefix(key);
        state.prefixTimer = setTimeout(clearPrefix, 700);
        return;
      }
      if (!bindings[key]) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      execute(bindings[key]);
    },
    true,
  );
  globalThis.__omawebKeyboardNavigation = {
    configure(configuration) {
      clearHints();
      clearPrefix();
      state.config = configuration;
    },
  };
})();
