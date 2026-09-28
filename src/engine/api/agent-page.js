// What the page verbs do inside a page (ADR 0051): the targets `look` labels,
// the outline it reads, the Markdown `read` returns, and the parts of a `do`
// step that are the page's to answer. It runs in the adapter's isolated world,
// where the page's own script cannot see it or change what it reports, and is
// installed once per document.
//
// A label names its element for as long as the document lives. The adapter
// hands in the next label it has not given out in this tab, so a label from a
// document that has gone never names an element of the one that replaced it.
(() => {
  if (globalThis.__omawebAgent) return;

  const labels = new Map();
  const labelOf = new WeakMap();
  let next = 1;
  let lastChange = performance.now();
  // What a page does to animate itself is not a change the Agent waits on: a
  // script animation writes an element's style, or its data, every frame, and
  // a page with one running would never be quiet.
  const animating = (name) => name === "style" || (name && name.startsWith("data-"));
  new MutationObserver((changes) => {
    if (changes.some((change) => change.type !== "attributes" || !animating(change.attributeName)))
      lastChange = performance.now();
  }).observe(document, { subtree: true, childList: true, attributes: true, characterData: true });

  // An outline near 1,500 tokens, at about four characters a token.
  const outlineCharacters = 6000;
  const lineCharacters = 120;
  const nameCharacters = 80;
  const maximumTargets = 200;
  const markdownCharacters = 200000;
  const valueCharacters = 1024 * 1024;
  const errorCharacters = 2000;

  const interactive = [
    "a[href]",
    "button",
    "input:not([type=hidden])",
    "select",
    "textarea",
    "summary",
    "[contenteditable='']",
    "[contenteditable='true']",
    "[role=button]",
    "[role=link]",
    "[role=checkbox]",
    "[role=radio]",
    "[role=switch]",
    "[role=tab]",
    "[role=menuitem]",
    "[role=menuitemcheckbox]",
    "[role=menuitemradio]",
    "[role=option]",
    "[role=combobox]",
    "[role=textbox]",
    "[role=searchbox]",
    "[role=slider]",
    "[role=spinbutton]",
  ].join(",");
  const focusable = "[tabindex]:not([tabindex='-1'])";
  const outlined = "h1,h2,h3,h4,h5,h6,p,li,dt,dd,blockquote,figcaption,caption,legend,pre,th,td";
  const typeable = new Set([
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

  const clean = (text, limit) => {
    const flat = String(text || "")
      .replace(/\s+/g, " ")
      .trim();
    return flat.length > limit ? flat.slice(0, limit - 1) + "…" : flat;
  };

  // Every element of the document, open shadow roots included, since a page
  // built from components keeps its controls there.
  const elements = function* (root) {
    for (const element of root.querySelectorAll("*")) {
      yield element;
      if (element.shadowRoot) yield* elements(element.shadowRoot);
    }
  };

  const shown = (element) => {
    const rect = element.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) return false;
    if (typeof element.checkVisibility === "function") {
      return element.checkVisibility({ checkOpacity: true, checkVisibilityCSS: true });
    }
    const style = getComputedStyle(element);
    return style.visibility !== "hidden" && style.display !== "none";
  };

  const isTarget = (element) => {
    if (element.matches(interactive)) return true;
    // Something a page made focusable by hand counts only where no control
    // around it is already the target.
    return element.matches(focusable) && !element.parentElement?.closest(interactive);
  };

  const labelFor = (element) => {
    let label = labelOf.get(element);
    if (label === undefined) {
      label = String(next++);
      labelOf.set(element, label);
      labels.set(label, new WeakRef(element));
    }
    return label;
  };

  const kindOf = (element) => {
    const role = element.getAttribute("role");
    if (role) return role;
    const tag = element.tagName.toLowerCase();
    if (tag === "a") return "link";
    if (tag === "input") {
      const type = String(element.type || "text").toLowerCase();
      if (type === "submit" || type === "button" || type === "reset" || type === "image")
        return "button";
      if (type === "range") return "slider";
      return type;
    }
    if (element.isContentEditable) return "editable";
    return tag;
  };

  // A label's own words, without those of the controls inside it: a select's
  // options are not its name.
  const labelText = (label) => {
    let text = "";
    const walk = (node) => {
      for (const child of node.childNodes) {
        if (child.nodeType === Node.TEXT_NODE) text += child.textContent;
        else if (
          child.nodeType === Node.ELEMENT_NODE &&
          !child.matches("input,select,textarea,button")
        )
          walk(child);
      }
    };
    walk(label);
    return text;
  };

  const textOf = (id) => {
    const element = document.getElementById(id);
    return element ? element.innerText || element.textContent : "";
  };

  const nameOf = (element) => {
    const labelled = element.getAttribute("aria-labelledby");
    if (labelled) return clean(labelled.split(/\s+/).map(textOf).join(" "), nameCharacters);
    const aria = element.getAttribute("aria-label");
    if (aria) return clean(aria, nameCharacters);
    if (element.labels && element.labels.length > 0) {
      return clean(Array.from(element.labels, labelText).join(" "), nameCharacters);
    }
    const tag = element.tagName;
    if (tag === "INPUT") {
      const type = String(element.type || "text").toLowerCase();
      if (type === "submit" || type === "button" || type === "reset")
        return clean(element.value, nameCharacters);
      if (type === "image") return clean(element.alt, nameCharacters);
    }
    if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") {
      return clean(element.placeholder || element.title || element.name, nameCharacters);
    }
    const text = clean(element.innerText, nameCharacters);
    if (text) return text;
    const image = element.querySelector("img[alt]");
    return clean((image && image.alt) || element.title, nameCharacters);
  };

  // What a control holds now. A password is only said to be filled.
  const describe = (element, label) => {
    const target = { label, kind: kindOf(element), name: nameOf(element) };
    const tag = element.tagName;
    if (tag === "SELECT") {
      target.value = clean(element.selectedOptions[0]?.text, nameCharacters);
    } else if (tag === "TEXTAREA" || (tag === "INPUT" && typeable.has(target.kind))) {
      if (element.value) {
        target.value =
          target.kind === "password" ? "•".repeat(8) : clean(element.value, nameCharacters);
      }
    }
    if (
      element.checked === true ||
      element.getAttribute("aria-checked") === "true" ||
      element.getAttribute("aria-selected") === "true"
    ) {
      target.checked = true;
    }
    if (element.disabled === true || element.getAttribute("aria-disabled") === "true") {
      target.disabled = true;
    }
    return target;
  };

  const inView = (rect) =>
    rect.bottom > 0 && rect.top < innerHeight && rect.right > 0 && rect.left < innerWidth;

  const outline = (all) => {
    const lines = [];
    let length = 0;
    const kept = new Set();
    for (const element of elements(document)) {
      if (!element.matches(outlined)) continue;
      // A list item's paragraph is said once, as the item.
      const around = element.parentElement?.closest(outlined);
      if (around && kept.has(around)) continue;
      if (!shown(element)) continue;
      if (!all && !inView(element.getBoundingClientRect())) continue;
      const text = clean(element.innerText, lineCharacters);
      if (!text) continue;
      kept.add(element);
      const heading = /^H([1-6])$/.exec(element.tagName);
      const line = heading ? "#".repeat(Number(heading[1])) + " " + text : text;
      if (length + line.length + 1 > outlineCharacters) {
        lines.push("…");
        break;
      }
      lines.push(line);
      length += line.length + 1;
    }
    return lines.join("\n");
  };

  const look = (all) => {
    const targets = [];
    let above = 0;
    let below = 0;
    let unlisted = 0;
    for (const element of elements(document)) {
      if (!isTarget(element) || !shown(element)) continue;
      const rect = element.getBoundingClientRect();
      if (!all && !inView(rect)) {
        if (rect.bottom <= 0) above++;
        else below++;
        continue;
      }
      if (targets.length >= maximumTargets) {
        unlisted++;
        continue;
      }
      targets.push(describe(element, labelFor(element)));
    }
    const answer = {
      title: document.title,
      url: location.href,
      outline: outline(all),
      targets,
      above,
      below: below + unlisted,
    };
    return answer;
  };

  const find = (label) => {
    const reference = labels.get(String(label));
    if (!reference) {
      return {
        code: "stale-label",
        error: `There is no label ${label} on this page. Labels go with the page they came from.`,
      };
    }
    const element = reference.deref();
    if (!element || !element.isConnected) {
      return { code: "stale-label", error: `Label ${label} names an element that has gone.` };
    }
    return { element };
  };

  // Whether a press at a point reaches the element, rather than whatever the
  // page stood in front of it. The element itself, anything inside it, or
  // anything around it all take the press as the element's.
  const reaches = (element, hit) => {
    if (!hit) return false;
    if (element === hit || element.contains(hit)) return true;
    for (let node = element; node; node = node.parentNode || node.host) {
      if (node === hit) return true;
    }
    const label = hit.closest && hit.closest("label");
    return !!label && label.control === element;
  };

  const point = (label) => {
    const found = find(label);
    if (!found.element) return found;
    const element = found.element;
    if (!inView(element.getBoundingClientRect())) {
      element.scrollIntoView({ block: "center", inline: "center", behavior: "instant" });
    }
    const rect = element.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) {
      return { code: "not-shown", error: `Label ${label} is not on show.` };
    }
    const x = Math.min(Math.max(rect.left + rect.width / 2, 0), innerWidth - 1);
    const y = Math.min(Math.max(rect.top + rect.height / 2, 0), innerHeight - 1);
    const hit = document.elementFromPoint(x, y);
    if (!reaches(element, hit)) {
      const cover = hit ? describe(hit, "") : null;
      return {
        code: "covered",
        error: `Label ${label} is covered by ${cover ? cover.kind + (cover.name ? ' "' + cover.name + '"' : "") : "something"}.`,
      };
    }
    return { x, y };
  };

  const editable = (element) =>
    element.tagName === "TEXTAREA" ||
    (element.tagName === "INPUT" && typeable.has(String(element.type || "text").toLowerCase())) ||
    element.isContentEditable;

  // The field takes the keyboard, with what it holds selected, so the keys
  // the adapter sends next replace it.
  const focusField = (label) => {
    const found = find(label);
    if (!found.element) return found;
    const element = found.element;
    if (!editable(element)) {
      return { code: "not-a-field", error: `Label ${label} is not a field to type in.` };
    }
    element.scrollIntoView({ block: "center", behavior: "instant" });
    element.focus();
    if (element.isContentEditable) {
      const range = document.createRange();
      range.selectNodeContents(element);
      const selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
    } else {
      element.select();
    }
    const active = element.getRootNode().activeElement;
    if (active !== element && !element.contains(active)) {
      return { code: "not-focused", error: `Label ${label} would not take the keyboard.` };
    }
    return { ok: true };
  };

  // A select's popup is drawn by the browser, out of the page's reach and so
  // out of reach of input sent to it. The option is chosen as the page's own
  // script would choose it, with the events a choice raises.
  const choose = (label, wanted) => {
    const found = find(label);
    if (!found.element) return found;
    const element = found.element;
    if (element.tagName !== "SELECT") {
      return {
        code: "not-a-select",
        error: `Label ${label} is not a select. Click its options instead.`,
      };
    }
    const options = Array.from(element.options);
    const want = String(wanted).trim();
    const option =
      options.find((each) => each.text.trim() === want || each.value === want) ||
      options.find((each) => each.text.trim().toLowerCase().includes(want.toLowerCase()));
    if (!option) {
      return { code: "no-option", error: `Label ${label} has no option "${want}".` };
    }
    element.value = option.value;
    element.dispatchEvent(new Event("input", { bubbles: true }));
    element.dispatchEvent(new Event("change", { bubbles: true }));
    return { ok: true };
  };

  const scroll = (target) => {
    const page = document.scrollingElement || document.documentElement;
    if (target === "up" || target === "down") {
      scrollBy({ top: (target === "up" ? -0.8 : 0.8) * innerHeight, behavior: "instant" });
      return { ok: true };
    }
    if (target === "top" || target === "bottom") {
      scrollTo({ top: target === "top" ? 0 : page.scrollHeight, behavior: "instant" });
      return { ok: true };
    }
    const found = find(target);
    if (!found.element) return found;
    found.element.scrollIntoView({ block: "center", behavior: "instant" });
    return { ok: true };
  };

  const present = (kind, value) =>
    kind === "url"
      ? location.href.includes(value)
      : (document.body?.innerText || "").includes(value);

  const skipped = new Set(["SCRIPT", "STYLE", "NOSCRIPT", "TEMPLATE", "HEAD", "SVG", "CANVAS"]);
  const blocks = new Set([
    "ADDRESS",
    "ARTICLE",
    "ASIDE",
    "DETAILS",
    "DIV",
    "DL",
    "FIELDSET",
    "FIGURE",
    "FOOTER",
    "FORM",
    "HEADER",
    "MAIN",
    "NAV",
    "P",
    "SECTION",
    "SUMMARY",
  ]);

  // The page as Markdown: its headings, paragraphs, lists, links, tables and
  // code, and nothing the page does not draw.
  const markdown = (root) => {
    const children = (node, context) =>
      Array.from(node.shadowRoot ? node.shadowRoot.childNodes : node.childNodes, (child) =>
        render(child, context),
      ).join("");
    const inline = (node) => children(node, { inline: true, depth: 0 }).replace(/\s+/g, " ").trim();
    const list = (node, context) => {
      const ordered = node.tagName === "OL";
      let index = 0;
      let text = "\n\n";
      for (const item of node.children) {
        if (item.tagName !== "LI" || !shown(item)) continue;
        index++;
        const marker = ordered ? `${index}. ` : "- ";
        const body = children(item, { inline: false, depth: context.depth + 1 })
          .replace(/\n{2,}/g, "\n")
          .trim()
          .replace(/\n/g, "\n" + "  ".repeat(context.depth + 1));
        text += "  ".repeat(context.depth) + marker + body + "\n";
      }
      return text + "\n";
    };
    const table = (node) => {
      const rows = Array.from(node.querySelectorAll("tr")).filter(shown);
      if (rows.length === 0) return "";
      const cells = rows.map((row) =>
        Array.from(row.children, (cell) => inline(cell).replace(/\|/g, "\\|")),
      );
      const width = Math.max(...cells.map((row) => row.length));
      const line = (row) =>
        "| " + Array.from({ length: width }, (_, index) => row[index] || "").join(" | ") + " |";
      return (
        "\n\n" +
        [line(cells[0]), line(Array(width).fill("---")), ...cells.slice(1).map(line)].join("\n") +
        "\n\n"
      );
    };
    const render = (node, context) => {
      if (node.nodeType === Node.TEXT_NODE) return node.textContent.replace(/\s+/g, " ");
      if (node.nodeType !== Node.ELEMENT_NODE) return "";
      const tag = node.tagName;
      if (skipped.has(tag.toUpperCase())) return "";
      if (tag !== "BR" && !shown(node)) return "";
      const heading = /^H([1-6])$/.exec(tag);
      if (heading) return "\n\n" + "#".repeat(Number(heading[1])) + " " + inline(node) + "\n\n";
      switch (tag) {
        case "BR":
          return "\n";
        case "HR":
          return "\n\n---\n\n";
        case "A": {
          const text = inline(node);
          const href = node.href;
          return href && text ? `[${text}](${href})` : text;
        }
        case "IMG":
          return node.alt ? `![${clean(node.alt, nameCharacters)}](${node.src})` : "";
        case "STRONG":
        case "B": {
          const text = inline(node);
          return text ? `**${text}**` : "";
        }
        case "EM":
        case "I": {
          const text = inline(node);
          return text ? `*${text}*` : "";
        }
        case "CODE":
          return "`" + node.textContent + "`";
        case "PRE":
          return "\n\n```\n" + node.innerText.replace(/\n+$/, "") + "\n```\n\n";
        case "UL":
        case "OL":
          return list(node, context);
        case "TABLE":
          return table(node);
        case "BLOCKQUOTE":
          return (
            "\n\n" +
            children(node, context)
              .trim()
              .split("\n")
              .map((line) => "> " + line)
              .join("\n") +
            "\n\n"
          );
        case "INPUT":
        case "SELECT":
        case "TEXTAREA":
          return "";
        default:
          if (blocks.has(tag) || tag === "LI" || tag === "DT" || tag === "DD") {
            return "\n\n" + children(node, context) + "\n\n";
          }
          return children(node, context);
      }
    };
    let text = render(root, { inline: false, depth: 0 })
      .replace(/[ \t]+\n/g, "\n")
      // A collapsed space left at the start of a line. List items indent by
      // two, which this leaves alone.
      .replace(/\n (?=\S)/g, "\n")
      .replace(/\n{3,}/g, "\n\n")
      .trim();
    if (text.length > markdownCharacters) {
      text = text.slice(0, markdownCharacters) + "\n\n(cut at 200,000 characters)";
    }
    return text;
  };

  const read = (selector) => {
    let root = document.body || document.documentElement;
    if (selector) {
      try {
        root = document.querySelector(selector);
      } catch (error) {
        return { code: "bad-request", error: `"${selector}" is not a selector.` };
      }
      if (!root) return { code: "not-found", error: `Nothing on the page matches "${selector}".` };
    }
    const title = document.title ? "# " + document.title + "\n\n" : "";
    return { markdown: (selector ? "" : title) + markdown(root) };
  };

  // What an expression came to, as JSON the adapter can carry back. A promise
  // is kept until it settles and collected by its number.
  const pending = new Map();
  let nextPending = 1;
  const settle = (value) => {
    let json;
    try {
      json = JSON.stringify(value === undefined ? null : value);
    } catch (error) {
      return { code: "not-json", error: `The value is not JSON: ${error.message}` };
    }
    if (json === undefined) json = "null";
    if (json.length > valueCharacters) {
      return { code: "too-large", error: `The value is ${json.length} characters of JSON.` };
    }
    return { json };
  };
  // What an expression threw, cut to a length an answer can carry.
  const threw = (error) => {
    let text;
    try {
      text = String((error && error.message) || error);
    } catch (failure) {
      text = "An error that cannot be written out.";
    }
    return { code: "threw", error: clean(text, errorCharacters) };
  };
  const evaluated = (value) => {
    if (value && typeof value.then === "function") {
      const id = nextPending++;
      const entry = { done: false };
      pending.set(id, entry);
      value.then(
        (result) => Object.assign(entry, { done: true }, settle(result)),
        (error) => Object.assign(entry, { done: true }, threw(error)),
      );
      return { pending: id };
    }
    return settle(value);
  };
  const collect = (id) => {
    const entry = pending.get(id);
    if (!entry) return { code: "failed", error: "The promise was lost with its page." };
    if (!entry.done) return { waiting: true };
    pending.delete(id);
    return entry;
  };

  globalThis.__omawebAgent = {
    // The adapter's next label, so labels count up across the tab's documents.
    begin(base) {
      next = Math.max(next, base);
    },
    next: () => next,
    look,
    read,
    point,
    focusField,
    choose,
    scroll,
    present,
    quiet: () => performance.now() - lastChange,
    evaluated,
    threw,
    collect,
    forget: (id) => pending.delete(id),
  };
})();
