// PROTOTYPE (#372): installed once per document in an isolated world. Throwaway.
(() => {
  if (globalThis.__agent) return true;

  const labels = new Map();
  const byElement = new WeakMap();
  let next = 0;
  const letters = "abcdefghijklmnopqrstuvwxyz";
  const makeLabel = () => {
    let i = next++;
    let s = "";
    do {
      s = letters[i % 26] + s;
      i = Math.floor(i / 26);
    } while (i > 0);
    return s.length < 2 ? "a" + s : s;
  };
  const label = (el) => {
    let l = byElement.get(el);
    if (!l) {
      l = makeLabel();
      byElement.set(el, l);
      labels.set(l, new WeakRef(el));
    }
    return l;
  };
  const get = (l) => {
    const el = labels.get(l)?.deref();
    if (!el || !el.isConnected) throw new Error("stale label " + l);
    return el;
  };

  const TARGETS = [
    "a[href]",
    "button",
    "input:not([type=hidden])",
    "select",
    "textarea",
    "summary",
    '[contenteditable=""]',
    '[contenteditable="true"]',
    "[onclick]",
    '[tabindex]:not([tabindex="-1"])',
    ...[
      "button",
      "link",
      "checkbox",
      "radio",
      "tab",
      "menuitem",
      "option",
      "switch",
      "combobox",
      "textbox",
    ].map((r) => `[role=${r}]`),
  ].join(",");

  const clip = (s, max) => {
    s = (s || "").replace(/\s+/g, " ").trim();
    return s.length > max ? s.slice(0, max - 1) + "…" : s;
  };
  const shown = (el) => {
    const r = el.getBoundingClientRect();
    if (r.width < 1 || r.height < 1) return null;
    const cs = getComputedStyle(el);
    if (cs.visibility === "hidden" || cs.display === "none") return null;
    return r;
  };
  const nameOf = (el) => {
    const aria = el.getAttribute("aria-label");
    if (aria) return aria;
    const by = el.getAttribute("aria-labelledby");
    if (by) {
      const t = by
        .split(/\s+/)
        .map((id) => document.getElementById(id)?.innerText || "")
        .join(" ");
      if (t.trim()) return t;
    }
    if (el.labels && el.labels.length) {
      // A wrapping <label> also holds the control, and a select's options are its text.
      const own = [...el.labels[0].childNodes]
        .filter((n) => n.nodeType === 3 || !n.matches("input,select,textarea,button"))
        .map((n) => n.textContent)
        .join(" ");
      if (own.trim()) return own;
    }
    const text = el.innerText;
    if (text && text.trim()) return text;
    return (
      el.getAttribute("placeholder") ||
      el.getAttribute("title") ||
      el.getAttribute("alt") ||
      el.querySelector?.("img[alt]")?.alt ||
      el.getAttribute("name") ||
      ""
    );
  };
  const kindOf = (el) => {
    const role = el.getAttribute("role");
    if (role) return role;
    const t = el.tagName.toLowerCase();
    if (t === "a") return "link";
    if (t === "input") return "input:" + (el.type || "text");
    if (el.isContentEditable) return "editable";
    return t;
  };
  const stateOf = (el) => {
    const t = el.tagName.toLowerCase();
    const out = [];
    if (t === "a" && el.href) {
      try {
        const u = new URL(el.href);
        const shortHref =
          u.origin === location.origin ? u.pathname + u.search + u.hash : u.host + u.pathname;
        out.push("→ " + clip(shortHref, 60));
      } catch {}
    }
    if (t === "input" || t === "textarea") {
      if (el.type === "checkbox" || el.type === "radio") out.push(el.checked ? "[x]" : "[ ]");
      else if (el.type === "password") {
        if (el.value) out.push("= ••••");
      } else if (el.value) out.push('= "' + clip(el.value, 40) + '"');
    }
    if (t === "select") out.push('= "' + clip(el.selectedOptions[0]?.text, 40) + '"');
    if (el.disabled || el.getAttribute("aria-disabled") === "true") out.push("(disabled)");
    for (const a of ["aria-expanded", "aria-checked", "aria-selected"]) {
      const v = el.getAttribute(a);
      if (v) out.push(a.slice(5) + "=" + v);
    }
    return out.join(" ");
  };

  const BLOCK = new Set(
    (
      "p div section article header footer nav aside main form fieldset figure " +
      "figcaption dl dt dd details summary address"
    ).split(" "),
  );
  const SKIP = new Set(
    "script style noscript template svg canvas iframe head input select textarea".split(" "),
  );
  const markdown = (node) => {
    if (node.nodeType === 3) return node.nodeValue.replace(/\s+/g, " ");
    if (node.nodeType !== 1) return "";
    const el = node;
    const t = el.tagName.toLowerCase();
    if (SKIP.has(t)) return "";
    const cs = getComputedStyle(el);
    if (cs.display === "none" || cs.visibility === "hidden") return "";
    const kids = () => [...el.childNodes].map(markdown).join("");
    if (/^h[1-6]$/.test(t)) return `\n\n${"#".repeat(+t[1])} ${kids().trim()}\n\n`;
    if (t === "ul" || t === "ol") {
      const items = [...el.children]
        .filter((c) => c.tagName === "LI")
        .map((li, i) => (t === "ol" ? `${i + 1}. ` : "- ") + markdown(li).trim());
      return "\n\n" + items.join("\n") + "\n\n";
    }
    if (t === "li") return kids();
    if (t === "a") {
      const s = kids().trim();
      const href = el.getAttribute("href");
      if (!s) return "";
      if (!href || href.startsWith("javascript:")) return s;
      return `[${s}](${el.href})`;
    }
    if (t === "img") return el.alt ? `![${clip(el.alt, 80)}]` : "";
    if (t === "pre") return "\n\n```\n" + el.innerText.replace(/\n$/, "") + "\n```\n\n";
    if (t === "code") return "`" + el.textContent + "`";
    if (t === "strong" || t === "b") {
      const s = kids().trim();
      return s ? `**${s}**` : "";
    }
    if (t === "em" || t === "i") {
      const s = kids().trim();
      return s ? `_${s}_` : "";
    }
    if (t === "br") return "\n";
    if (t === "hr") return "\n\n---\n\n";
    if (t === "blockquote") return "\n\n> " + kids().trim().replace(/\n/g, "\n> ") + "\n\n";
    if (t === "table") {
      const rows = [...el.rows].map(
        (r) =>
          "| " +
          [...r.cells].map((c) => clip(c.innerText, 80).replace(/\|/g, "\\|")).join(" | ") +
          " |",
      );
      if (rows.length > 1) {
        const cells = el.rows[0].cells.length;
        rows.splice(1, 0, "|" + " --- |".repeat(cells));
      }
      return "\n\n" + rows.join("\n") + "\n\n";
    }
    if (BLOCK.has(t)) return `\n\n${kids().trim()}\n\n`;
    return kids();
  };

  const fire = (el, ...types) => {
    for (const type of types) el.dispatchEvent(new Event(type, { bubbles: true }));
  };

  globalThis.__agent = {
    look(all) {
      const vw = innerWidth;
      const vh = innerHeight;
      let above = 0;
      let below = 0;
      const targets = [];
      for (const el of document.querySelectorAll(TARGETS)) {
        const r = shown(el);
        if (!r) continue;
        const inView = r.bottom > 0 && r.top < vh && r.right > 0 && r.left < vw;
        if (!all && !inView) {
          if (r.bottom <= 0) above++;
          else below++;
          continue;
        }
        const name = clip(nameOf(el), 60);
        const state = stateOf(el);
        targets.push(
          label(el) + " " + kindOf(el) + (name ? ` "${name}"` : "") + (state ? " " + state : ""),
        );
      }
      const heads = [...document.querySelectorAll("h1,h2,h3")]
        .filter(shown)
        .slice(0, 12)
        .map((h) => "#".repeat(+h.tagName[1]) + " " + clip(h.innerText, 80));
      const root = document.querySelector("main,[role=main],article") || document.body;
      return {
        title: document.title,
        url: location.href,
        visibility: document.visibilityState,
        heads,
        text: clip(root?.innerText, 400),
        targets,
        above,
        below,
        scroll: `${Math.round(scrollY)}/${Math.max(0, document.documentElement.scrollHeight - vh)}`,
      };
    },
    target(l) {
      const el = get(l);
      el.scrollIntoView({ block: "center", inline: "center", behavior: "instant" });
      const r = el.getBoundingClientRect();
      const x = r.left + r.width / 2;
      const y = r.top + r.height / 2;
      const hit = document.elementFromPoint(x, y);
      const reachable = !!hit && (hit === el || el.contains(hit) || hit.contains(el));
      return { x, y, reachable, visibility: document.visibilityState };
    },
    click(l) {
      get(l).click();
      return true;
    },
    focus(l) {
      const el = get(l);
      const typed =
        el.isContentEditable ||
        el.tagName === "TEXTAREA" ||
        (el.tagName === "INPUT" &&
          !/^(checkbox|radio|button|submit|reset|file|image|range|color)$/.test(el.type));
      if (!typed) throw new Error(`${l} is not a text field; use click or select`);
      el.focus();
      if (el.select) {
        try {
          el.select();
        } catch {}
      } else if (el.isContentEditable) {
        const range = document.createRange();
        range.selectNodeContents(el);
        const sel = getSelection();
        sel.removeAllRanges();
        sel.addRange(range);
      }
      return document.activeElement === el || el.contains(document.activeElement);
    },
    value(l) {
      const el = get(l);
      return el.isContentEditable ? el.innerText : el.value;
    },
    setValue(l, v) {
      const el = get(l);
      if (el.isContentEditable) el.textContent = v;
      else Object.getOwnPropertyDescriptor(Object.getPrototypeOf(el), "value").set.call(el, v);
      fire(el, "input", "change");
      return true;
    },
    select(l, v) {
      const el = get(l);
      const opts = [...el.options];
      const low = v.toLowerCase();
      const o =
        opts.find((o) => o.value === v) ||
        opts.find((o) => o.text.trim().toLowerCase() === low) ||
        opts.find((o) => o.text.toLowerCase().includes(low));
      if (!o) throw new Error("no option matching " + v);
      el.value = o.value;
      fire(el, "input", "change");
      return o.text.trim();
    },
    scroll(where) {
      if (where === "down") scrollBy(0, innerHeight * 0.8);
      else if (where === "up") scrollBy(0, -innerHeight * 0.8);
      else if (where === "top") scrollTo(0, 0);
      else if (where === "bottom") scrollTo(0, document.documentElement.scrollHeight);
      else get(where).scrollIntoView({ block: "center", behavior: "instant" });
      return true;
    },
    quiet(ms, max) {
      return new Promise((resolve) => {
        const start = performance.now();
        let last = start;
        const mo = new MutationObserver(() => {
          last = performance.now();
        });
        mo.observe(document, {
          subtree: true,
          childList: true,
          attributes: true,
          characterData: true,
        });
        const tick = () => {
          const now = performance.now();
          if (now - last >= ms || now - start >= max) {
            mo.disconnect();
            resolve({ settled: now - last >= ms, waited: Math.round(now - start) });
          } else setTimeout(tick, 50);
        };
        setTimeout(tick, 50);
      });
    },
    has(text) {
      return !!document.body && document.body.innerText.includes(text);
    },
    read(selector, offset, max) {
      const root = selector
        ? document.querySelector(selector)
        : document.querySelector("main,[role=main],article") || document.body;
      if (!root) throw new Error("nothing matches " + selector);
      const md = markdown(root)
        .replace(/[ \t]+\n/g, "\n")
        .replace(/\n[ \t]+/g, "\n")
        .replace(/\n{3,}/g, "\n\n")
        .trim();
      return { md: md.slice(offset, offset + max), total: md.length };
    },
  };
  return true;
})();
