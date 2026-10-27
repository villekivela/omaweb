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
    overlay: null,
    indicator: null,
  };
  // What this script puts in the page is found again by reference. An id is
  // only a handle for whoever drives the browser, and is the token the shell
  // draws for its view and a letter, so a page sees no name of Omaweb's: "s"
  // for the key sequence, "h" for the hint layer and "i" for its input.
  const name = (element, letter) => {
    if (state.config.elementToken) element.id = state.config.elementToken + letter;
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
  // The box a hint is drawn at, and the box a click on the element would land
  // in: the first of the element's client rects, cropped to the viewport, that
  // is at least 3x3 px on an element that is visible. A link that wraps has one
  // rect a line, so its first visible line is the one found. Vimium's
  // getVisibleClientRect, without its walk into the children of an empty box.
  const minimumSide = 3;
  const visibleRect = (element) => {
    if (getComputedStyle(element).visibility !== "visible") return null;
    for (const rect of element.getClientRects()) {
      const left = Math.max(rect.left, 0);
      const top = Math.max(rect.top, 0);
      const right = Math.min(rect.right, innerWidth);
      const bottom = Math.min(rect.bottom, innerHeight);
      if (right - left < minimumSide || bottom - top < minimumSide) continue;
      return { left, top, right, bottom };
    }
    return null;
  };
  // Whether the element is what the reader would reach by clicking where it
  // stands: it, something inside it or something around it is on top at the
  // middle of the box or at one of its corners. A corner is taken 0.1 px in,
  // where the edge itself can belong to the neighbour. A dialog's backdrop, a
  // menu or a sticky header over the whole box leaves none of these.
  // What is on top is looked for inside each shadow root on the way down, as
  // Vimium's getElementFromPoint does, since contains() does not cross into one.
  const elementFromPoint = (x, y, root = document, stack = []) => {
    const element = root.elementsFromPoint(x, y)[0];
    if (stack.includes(element)) return element;
    stack.push(element);
    if (element && element.shadowRoot) {
      // A shadow root can hold nothing but text, and then the host is the answer.
      return elementFromPoint(x, y, element.shadowRoot, stack) || element;
    }
    return element;
  };
  const reachable = (element, rect) => {
    const onTop = (x, y) => {
      const top = elementFromPoint(x, y);
      return Boolean(top) && (element.contains(top) || top.contains(element));
    };
    if (onTop((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2)) return true;
    return [rect.top + 0.1, rect.bottom - 0.1].some((y) =>
      [rect.left + 0.1, rect.right - 0.1].some((x) => onTop(x, y)),
    );
  };
  // A control the page switched off, or made read-only, is not one to click
  // into.
  const switchedOff = (element) => {
    const ariaDisabled = element.getAttribute("aria-disabled");
    return (
      element.matches(":disabled") ||
      (field(element) && element.readOnly === true) ||
      (ariaDisabled !== null && ["", "true"].includes(ariaDisabled.toLowerCase()))
    );
  };
  // Which elements take a hint follows Vimium's link hints: getLocalHintsForElement,
  // getLocalHints and getElementFromPoint in content_scripts/link_hints.js, and isSelectable in
  // lib/dom_utils.js, of philc/vimium at commit 5aa29614bf1dce05e0d316f8c38722e17f9b38c3.
  // Copyright (c) 2010 Phil Crosby, Ilya Sukhar, under the MIT License, whose text is in
  // THIRD_PARTY_NOTICES.md. Left out are the hints Vimium gives for its own ends: a frame's body
  // to focus the frame, a box that scrolls, and the areas of an image map. Omaweb's own checks
  // stay around the rules: switchedOff, visibleRect and reachable.
  const unselectableTypes = [
    "button",
    "checkbox",
    "color",
    "file",
    "hidden",
    "image",
    "radio",
    "reset",
    "submit",
  ];
  const selectable = (element) =>
    element instanceof Element &&
    ((element.nodeName.toLowerCase() === "input" && !unselectableTypes.includes(element.type)) ||
      element.nodeName.toLowerCase() === "textarea" ||
      element.isContentEditable);
  const clickableRoles = [
    "button",
    "tab",
    "link",
    "checkbox",
    "menuitem",
    "menuitemcheckbox",
    "menuitemradio",
    "radio",
    "textbox",
  ];
  const angularClickAttributes = ["", "data-", "x-"].flatMap((prefix) =>
    ["-", ":", "_"].map((separator) => prefix + "ng" + separator + "click"),
  );
  // Whether the page is AngularJS's, asked once a hint run as Vimium does.
  let angularPage = false;
  // An element with a tabindex and nothing else is a second-class citizen, and
  // getLocalHints drops every one, so a tabindex alone still earns no hint.
  // An element found only by its class name, and any span, is a possible false
  // positive, which a clickable element just inside it replaces.
  const localHint = (element) => {
    // `tagName` can be an element a form named so (Vimium #2035).
    const tagName = element.tagName.toLowerCase?.() || "";
    let isClickable = false;
    let possibleFalsePositive = false;
    if (switchedOff(element)) return null;
    if (
      angularPage &&
      angularClickAttributes.some((attribute) => element.hasAttribute(attribute))
    ) {
      isClickable = true;
    }
    if (element.hasAttribute("onclick")) {
      isClickable = true;
    } else {
      const role = element.getAttribute("role");
      if (role !== null && clickableRoles.includes(role.toLowerCase())) {
        isClickable = true;
      } else {
        const contentEditable = element.getAttribute("contentEditable");
        if (
          contentEditable !== null &&
          ["", "contenteditable", "true"].includes(contentEditable.toLowerCase())
        ) {
          isClickable = true;
        }
      }
    }
    if (!isClickable && element.hasAttribute("jsaction")) {
      for (const jsactionRule of element.getAttribute("jsaction").split(";")) {
        const ruleSplit = jsactionRule.trim().split(":");
        if (ruleSplit.length >= 1 && ruleSplit.length <= 2) {
          const [eventType, namespace, actionName] =
            ruleSplit.length === 1
              ? ["click", ...ruleSplit[0].trim().split("."), "_"]
              : [ruleSplit[0], ...ruleSplit[1].trim().split("."), "_"];
          if (!isClickable) {
            isClickable = eventType === "click" && namespace !== "none" && actionName !== "_";
          }
        }
      }
    }
    switch (tagName) {
      case "a":
        isClickable = true;
        break;
      case "textarea":
        isClickable ||= !element.disabled && !element.readOnly;
        break;
      case "input":
        isClickable ||= !(
          element.getAttribute("type")?.toLowerCase() === "hidden" ||
          element.disabled ||
          (element.readOnly && selectable(element))
        );
        break;
      case "button":
      case "select":
        isClickable ||= !element.disabled;
        break;
      case "object":
      case "embed":
        isClickable = true;
        break;
      case "label":
        isClickable ||=
          element.control !== null &&
          !element.control.disabled &&
          localHint(element.control) === null;
        break;
      case "img":
        isClickable ||= ["zoom-in", "zoom-out"].includes(element.style.cursor);
        break;
      case "details":
        isClickable = true;
        break;
    }
    if (!isClickable) {
      const className = element.getAttribute("class")?.toLowerCase();
      if (className?.includes("button") || className?.includes("btn")) {
        isClickable = true;
        possibleFalsePositive = true;
      }
    }
    if (tagName === "span") possibleFalsePositive = true;
    let secondClassCitizen = false;
    const tabIndexValue = element.getAttribute("tabindex");
    const tabIndex = tabIndexValue ? parseInt(tabIndexValue) : -1;
    if (!isClickable && !(tabIndex < 0) && !isNaN(tabIndex)) {
      isClickable = true;
      secondClassCitizen = true;
    }
    if (!isClickable) return null;
    const rect = visibleRect(element);
    if (rect === null) return null;
    return { target: element, rect, secondClassCitizen, possibleFalsePositive };
  };
  // Every element in document order, shadow roots included, which is the order
  // that tells which of two overlapping elements is on top.
  const allElements = (root, elements = []) => {
    for (const element of root.querySelectorAll("*")) {
      elements.push(element);
      if (element.shadowRoot) allElements(element.shadowRoot, elements);
    }
    return elements;
  };
  const hintTargets = (requireHref) => {
    angularPage = document.getElementsByClassName("ng-scope").length > 0;
    let hints = allElements(document.documentElement)
      .filter((element) => !requireHref || Boolean(element.href))
      .map(localHint)
      .filter(Boolean)
      .reverse();
    // A possible false positive is dropped when one of the six hints before it,
    // descendants come first now, sits up to three levels inside it.
    const descendantsToCheck = 3;
    const lookbackWindow = 6;
    hints = hints.filter((hint, position) => {
      if (!hint.possibleFalsePositive) return true;
      for (let index = Math.max(0, position - lookbackWindow); index < position; index++) {
        let candidateDescendant = hints[index].target;
        for (let level = 0; level < descendantsToCheck; level++) {
          candidateDescendant = candidateDescendant?.parentElement;
          if (candidateDescendant === hint.target) return false;
        }
      }
      return true;
    });
    return hints
      .filter((hint) => !hint.secondClassCitizen && reachable(hint.target, hint.rect))
      .reverse();
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
    state.overlay?.remove();
    state.overlay = null;
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
    state.indicator?.remove();
    state.indicator = null;
  };
  const showPrefix = (key) => {
    state.indicator?.remove();
    const indicator = document.createElement("span");
    name(indicator, "s");
    state.indicator = indicator;
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
    const candidates = hintTargets(command === "open-link-background");
    const overlay = document.createElement("div");
    name(overlay, "h");
    overlay.setAttribute("role", "status");
    overlay.setAttribute("aria-live", "polite");
    overlay.setAttribute("aria-label", candidates.length + " link hints available");
    const theme = state.config.hintTheme || {};
    const font = theme.font || {};
    const fontFamily = font.family || (font.families && font.families[0]) || "system-ui";
    const fontSize = Math.max(11, Number(font.size) || 12);
    // A popover is shown in the top layer, above a modal dialog's, which no
    // z-index reaches. Its own box is undone: it is only a frame for the plates.
    overlay.popover = "manual";
    overlay.style.cssText =
      "position:fixed;inset:0;width:auto;height:auto;margin:0;padding:0;border:0;" +
      "background:none;overflow:visible;pointer-events:none;z-index:2147483647;" +
      "color:" +
      (theme.text || "CanvasText") +
      ";font-family:" +
      JSON.stringify(fontFamily) +
      ",monospace;font-size:" +
      fontSize +
      "px;font-weight:600;";
    const focus = document.createElement("input");
    name(focus, "i");
    focus.type = "text";
    focus.tabIndex = -1;
    focus.setAttribute("aria-label", "Type a link hint");
    focus.setAttribute("autocomplete", "off");
    focus.style.cssText =
      "position:fixed;left:-2px;top:-2px;width:1px;height:1px;" +
      "opacity:.001;pointer-events:none;";
    overlay.append(focus);
    const labels = labelsFor(candidates.length);
    state.hints = candidates.map(({ target, rect }, index) => {
      const label = labels[index];
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
      // The website's `.hint`: an accent plate with the code in the window's
      // ground on it, square, in uppercase, with no border and no shadow.
      hint.style.cssText =
        "position:absolute;left:" +
        rect.left +
        "px;top:" +
        rect.top +
        "px;padding:2px 4px;border:0;border-radius:0;background:" +
        (theme.accent || "Highlight") +
        ";color:" +
        (theme.windowOpaque || theme.surface || "Canvas") +
        ";text-transform:uppercase;line-height:1;forced-color-adjust:auto;";
      for (const character of label.toUpperCase()) {
        const part = document.createElement("span");
        part.textContent = character;
        hint.append(part);
      }
      overlay.append(hint);
      return { label, target, hint };
    });
    // A modal dialog makes all that lies outside it inert, a popover included,
    // and the input in it could not take the focus. Inside the dialog the layer
    // is still in the top layer, and the input is live.
    (document.querySelector("dialog:modal") ?? document.documentElement).append(overlay);
    overlay.showPopover();
    state.overlay = overlay;
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
    } else if (target.tagName === "DETAILS") {
      // A click on a details element outside its summary opens nothing, so the
      // hint toggles it, as Vimium's does.
      target.open = !target.open;
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
