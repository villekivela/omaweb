// What a procedural cosmetic rule does to the elements it names
// (docs/adr/0052-apply-procedural-cosmetic-filters.md). Brave's vendored
// matcher, loaded ahead of this into the same application world, finds the
// elements; this hides, restyles or removes them, watches the document for
// more, and undoes what it can when the rules change under the page.
//
// It runs in the application world, so the page cannot reach its state or
// replace the MutationObserver it relies on. The DOM is shared, so a hide is a
// marker attribute whose name is drawn for each document and one stylesheet
// rule that matches it, rather than an inline style the page's own style
// changes would fight.
(() => {
  if (globalThis.__omawebProcedural) return;
  const filters = globalThis.omawebProceduralFilters;
  // How often the rules may run again while the page changes.
  const interval = 100;
  const marker =
    "x" +
    Array.from(crypto.getRandomValues(new Uint32Array(3)), (part) => part.toString(36)).join("");

  // Which rules a mutation of each kind can make match. A tree change can
  // bring any element in, so it runs them all.
  const textOperators = new Set(["has-text", "min-text-length"]);
  const attributeOperators = new Set([
    "matches-attr",
    "matches-css",
    "matches-css-before",
    "matches-css-after",
  ]);
  const attributeActions = new Set(["remove-attr", "remove-class"]);
  const kindsOf = (action) => {
    const kinds = new Set(["tree"]);
    for (const operator of action.selector) {
      if (textOperators.has(operator.type)) kinds.add("text");
      if (attributeOperators.has(operator.type)) kinds.add("attribute");
    }
    if (action.action && attributeActions.has(action.action.type)) kinds.add("attribute");
    return kinds;
  };

  let rules = [];
  let running = false;
  let sheet = null;
  let observer = null;
  let timer = 0;
  let ranAt = -Infinity;
  let pending = new Set();
  const marked = new Set();
  const removedAttributes = [];
  const removedClasses = [];

  const mark = (element, token) => {
    const tokens = (element.getAttribute(marker) || "").split(" ").filter(Boolean);
    if (!tokens.includes(token)) {
      tokens.push(token);
      element.setAttribute(marker, tokens.join(" "));
    }
    marked.add(element);
  };

  const act = (rule, element) => {
    const action = rule.action;
    if (!action) {
      mark(element, "h");
    } else if (action.type === "style") {
      mark(element, rule.token);
    } else if (action.type === "remove") {
      element.remove();
    } else if (action.type === "remove-attr") {
      if (element.hasAttribute(action.arg)) {
        removedAttributes.push([element, action.arg, element.getAttribute(action.arg)]);
        element.removeAttribute(action.arg);
      }
    } else if (action.type === "remove-class") {
      if (element.classList.contains(action.arg)) {
        removedClasses.push([element, action.arg]);
        element.classList.remove(action.arg);
      }
    }
  };

  // The page may have dropped the sheet from its adopted list since the last
  // run, so each run puts it back.
  const adoptSheet = () => {
    if (sheet && !document.adoptedStyleSheets.includes(sheet))
      document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];
  };

  const run = (kinds) => {
    adoptSheet();
    for (const rule of rules) {
      if (kinds && !rule.kinds.some((kind) => kinds.has(kind))) continue;
      try {
        for (const element of filters.applyCompiledSelector(rule.compiled)) act(rule, element);
      } catch (error) {
        // A rule the matcher cannot evaluate here, such as a regular
        // expression this engine refuses, hides nothing rather than stopping
        // the rules after it.
      }
    }
  };

  const flush = () => {
    timer = 0;
    ranAt = performance.now();
    const kinds = pending;
    pending = new Set();
    run(kinds);
  };

  const note = (kind) => {
    pending.add(kind);
    if (timer) return;
    timer = setTimeout(flush, Math.max(0, ranAt + interval - performance.now()));
  };

  const stop = () => {
    running = false;
    observer?.disconnect();
    observer = null;
    clearTimeout(timer);
    timer = 0;
    pending = new Set();
    for (const element of marked) element.removeAttribute(marker);
    marked.clear();
    for (const [element, name, value] of removedAttributes.splice(0))
      if (!element.hasAttribute(name)) element.setAttribute(name, value);
    for (const [element, name] of removedClasses.splice(0)) element.classList.add(name);
    if (sheet) document.adoptedStyleSheets = document.adoptedStyleSheets.filter((s) => s !== sheet);
    sheet = null;
    rules = [];
  };

  // Replaces the rules in force with these, undoing what the last ones did.
  // With `onlyIfIdle` it leaves rules that are already running alone, for the
  // second of two routes that both deliver a frame's first rules.
  const start = (actions, onlyIfIdle) => {
    if (onlyIfIdle && running) return;
    stop();
    rules = actions.flatMap((action, index) => {
      try {
        return [
          {
            compiled: filters.compileProceduralSelector(action.selector),
            action: action.action,
            kinds: [...kindsOf(action)],
            token: "s" + index,
          },
        ];
      } catch (error) {
        return [];
      }
    });
    if (rules.length === 0) return;
    running = true;

    const css = [`[${marker}~="h"] { display: none !important; }`];
    for (const rule of rules)
      if (rule.action && rule.action.type === "style")
        css.push(`[${marker}~="${rule.token}"] { ${rule.action.arg} }`);
    sheet = new CSSStyleSheet();
    sheet.replaceSync(css.join("\n"));

    const kinds = new Set(rules.flatMap((rule) => rule.kinds));
    observer = new MutationObserver((records) => {
      for (const record of records) {
        if (record.type === "childList") note("tree");
        else if (record.type === "characterData") note("text");
        else if (record.attributeName !== marker) note("attribute");
      }
    });
    observer.observe(document, {
      childList: true,
      subtree: true,
      characterData: kinds.has("text"),
      attributes: kinds.has("attribute"),
    });
    ranAt = performance.now();
    run(null);
  };

  globalThis.__omawebProcedural = {
    start,
    stop,
    get running() {
      return running;
    },
  };
})();
