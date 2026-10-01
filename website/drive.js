// The landing page's behaviour: the road behind it, the Omnibar on the Start page, the
// introduction and the theme; and on a release page, the road as a still header. Optional, like
// all of the page's script: without it the road is a gradient, the Omnibar's rows are links to
// their cards, and the introduction is an ordinary video.

import { createCrtRoad } from "./crt-road.js";
import { SceneHost } from "./scene.js";

// The road's parameters, shared with the browser's own road and served beside this script. A
// page that cannot fetch them keeps the gradient the road would have stood on.
const crtRoad = await fetch(new URL("crt-road.json", import.meta.url))
  .then((response) => (response.ok ? response.json() : Promise.reject(response.status)))
  .then(createCrtRoad)
  .catch(() => null);

const drive = document.querySelector(".drive");
if (drive) {
  const smooth = !matchMedia("(prefers-reduced-motion: reduce)").matches;
  const typing = (event) => event.target.closest("input, textarea, select, [contenteditable]");
  const canvas = drive.querySelector(".drive__scene canvas");
  const host = crtRoad ? new SceneHost(canvas, crtRoad) : { setNavigating() {} };

  // Past the Start page the road sinks under a scrim of the theme's ground so the cards read
  // against it, and the scroll's speed is how hard the reader is navigating: the road speeds up
  // with it and eases back when the scroll stops.
  const start = drive.querySelector(".start");
  const scrim = drive.querySelector(".drive__scrim");
  let lastY = scrollY;
  let lastAt = performance.now();
  let pace = 0;
  let easing = 0;
  const settle = () => {
    pace *= 0.9;
    host.setNavigating(pace);
    easing = pace > 0.01 ? requestAnimationFrame(settle) : 0;
  };
  const scrolled = () => {
    const now = performance.now();
    pace = Math.max(pace, Math.min(1, Math.abs(scrollY - lastY) / Math.max(16, now - lastAt) / 3));
    lastY = scrollY;
    lastAt = now;
    host.setNavigating(pace);
    if (!easing) easing = requestAnimationFrame(settle);
    scrim.style.opacity = String(Math.min(1, scrollY / (start.offsetHeight * 0.7)));
  };
  addEventListener("scroll", scrolled, { passive: true });
  scrolled();

  // The introduction plays muted while it is on screen and stops when it leaves. Under reduced
  // motion it waits on its poster for the reader to press play.
  const film = drive.querySelector(".film__video");
  if (film) {
    film.removeAttribute("controls");
    if (smooth) {
      new IntersectionObserver(
        ([entry]) => {
          if (entry.isIntersecting) film.play().catch(() => {});
          else film.pause();
        },
        { threshold: 0.5 },
      ).observe(film);
    } else {
      const play = drive.querySelector(".film__play");
      play.hidden = false;
      play.addEventListener("click", () => {
        play.hidden = true;
        film.controls = true;
        film.play().catch(() => {});
      });
    }
  }

  // The Omnibar: type to filter the features, arrows to choose a row, Return to go to its card.
  const omnibar = drive.querySelector(".omnibar");
  const input = omnibar.querySelector(".omnibar__input");
  const rows = [...omnibar.querySelectorAll('[role="option"]')];
  let chosen = 0;
  const shown = () => rows.filter((row) => !row.hidden);
  const choose = (index) => {
    const list = shown();
    if (!list.length) return;
    chosen = (index + list.length) % list.length;
    for (const row of rows) row.setAttribute("aria-selected", String(row === list[chosen]));
    input.setAttribute("aria-activedescendant", list[chosen].id);
  };
  const go = (row) => {
    const card = drive.querySelector(`#${row.querySelector("a").dataset.card}`);
    card.scrollIntoView({ behavior: smooth ? "smooth" : "auto", block: "center" });
  };
  input.addEventListener("input", () => {
    const query = input.value.trim().toLowerCase();
    for (const row of rows)
      row.hidden = Boolean(query) && !row.textContent.toLowerCase().includes(query);
    choose(0);
  });
  input.addEventListener("keydown", (event) => {
    if (event.key === "ArrowDown" || event.key === "ArrowUp") {
      event.preventDefault();
      choose(chosen + (event.key === "ArrowDown" ? 1 : -1));
    } else if (event.key === "Enter" && shown().length) {
      event.preventDefault();
      go(shown()[chosen]);
    } else if (event.key === "Escape") {
      input.value = "";
      input.dispatchEvent(new Event("input"));
      input.blur();
    }
  });
  for (const row of rows) {
    row.addEventListener("pointerenter", () => choose(shown().indexOf(row)));
    row.querySelector("a").addEventListener("click", (event) => {
      event.preventDefault();
      go(row);
    });
  }
  omnibar.querySelector(".omnibar__placeholder").remove();
  input.hidden = false;

  // The theme: the whole page, its road and its captures take the chosen Omarchy theme. A capture
  // with no data-shot shows a theme of its own, as the theme card's do.
  const captures = [...drive.querySelectorAll("img[data-shot]")];
  let theme = document.documentElement.dataset.theme;
  const picker = omnibar.querySelector(".themes");
  const swatches = [...picker.querySelectorAll("[data-theme-choice]")];
  // The pick lasts the visit, so the release pages open in it, and a new visit opens in Retro 82
  // again; theme.js applies it.
  const paint = (next, remember = true) => {
    theme = next;
    document.documentElement.dataset.theme = theme;
    if (remember) {
      try {
        sessionStorage.setItem("omaweb-theme", theme);
      } catch {
        // Storage can be blocked; the pick then lasts as long as the page.
      }
    }
    for (const swatch of swatches) {
      const on = swatch.dataset.themeChoice === theme;
      swatch.setAttribute("aria-pressed", String(on));
      if (on) picker.querySelector(".themes__name").textContent = swatch.dataset.name;
    }
    for (const image of captures) {
      image.src = `assets/shots/${theme}/${image.dataset.shot}.webp`;
    }
  };
  for (const swatch of swatches) {
    swatch.addEventListener("click", () => paint(swatch.dataset.themeChoice));
  }
  picker.hidden = false;
  paint(theme, false);

  // J and K: the next and the previous card, as they move through a Space's tabs. The card in
  // hand is the one last moved to while it is still on screen, else the one nearest the middle.
  const cards = [...drive.querySelectorAll(".card")];
  let current = -1;
  const step = (by) => {
    const onScreen = (card) => {
      const rect = card.getBoundingClientRect();
      return rect.bottom > 0 && rect.top < innerHeight;
    };
    if (!cards[current] || !onScreen(cards[current])) {
      const middle = innerHeight / 2;
      const below = cards.findIndex((card) => card.getBoundingClientRect().top > middle);
      current = below < 0 ? cards.length : below;
      if (by > 0) current -= 1;
    }
    current = Math.max(0, Math.min(cards.length - 1, current + by));
    for (const card of cards) card.toggleAttribute("data-current", card === cards[current]);
    cards[current].scrollIntoView({ behavior: smooth ? "smooth" : "auto", block: "center" });
  };

  // f: link hints. Every link and button on screen takes a label; typing a label follows it, and
  // any other key puts the hints away. The swatches are left to T, being too small to label.
  const LETTERS = "asdfghjkl";
  let hints = null;
  const hideHints = () => {
    for (const hint of hints || []) hint.label.remove();
    hints = null;
  };
  const showHints = () => {
    const targets = [...document.querySelectorAll("a[href], button:not(.swatch)")].filter(
      (target) => {
        const rect = target.getBoundingClientRect();
        return (
          rect.width && rect.height && rect.bottom > 0 && rect.top < innerHeight && rect.right > 0
        );
      },
    );
    const long = targets.length > LETTERS.length;
    hints = targets.map((target, at) => {
      const text = long
        ? LETTERS[Math.floor(at / LETTERS.length)] + LETTERS[at % LETTERS.length]
        : LETTERS[at];
      const rect = target.getBoundingClientRect();
      const label = document.createElement("span");
      label.className = "hint";
      label.textContent = text;
      label.style.setProperty("--x", `${rect.left + scrollX}px`);
      label.style.setProperty("--y", `${rect.top + scrollY}px`);
      document.body.append(label);
      return { target, text, label };
    });
  };
  let typed = "";
  const followHint = (key) => {
    typed += key;
    const left = hints.filter((hint) => hint.text.startsWith(typed));
    for (const hint of hints) hint.label.hidden = !left.includes(hint);
    if (left.length === 1 && left[0].text === typed) {
      hideHints();
      left[0].target.focus({ preventScroll: true });
      left[0].target.click();
    } else if (!left.length) {
      hideHints();
    }
  };

  // The browser's own keys: o to the Omnibar, f for link hints, J and K through the cards, T to the
  // next theme and Shift+T to the one before.
  addEventListener("keydown", (event) => {
    if (typing(event) || event.ctrlKey || event.metaKey || event.altKey) return;
    if (hints) {
      event.preventDefault();
      if (LETTERS.includes(event.key)) followHint(event.key);
      else hideHints();
    } else if (event.key === "f") {
      event.preventDefault();
      typed = "";
      showHints();
    } else if (event.key.toLowerCase() === "j" || event.key.toLowerCase() === "k") {
      event.preventDefault();
      step(event.key.toLowerCase() === "j" ? 1 : -1);
    } else if (event.key === "o") {
      event.preventDefault();
      scrollTo({ top: 0, behavior: smooth ? "smooth" : "auto" });
      input.focus({ preventScroll: true });
    } else if (event.key.toLowerCase() === "t") {
      const at = swatches.findIndex((swatch) => swatch.dataset.themeChoice === theme);
      const by = event.shiftKey ? -1 : 1;
      paint(swatches[(at + by + swatches.length) % swatches.length].dataset.themeChoice);
    }
  });
}

// A release page: the road as a thin header, one still frame without the glass, in the reader's
// palette.
const header = document.querySelector(".log__road canvas");
if (header && crtRoad) new SceneHost(header, crtRoad, { held: true });
