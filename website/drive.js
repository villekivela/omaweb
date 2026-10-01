// The landing page's behaviour: the road behind it, the Omnibar on the Start page, the
// introduction and the theme; and on a release page, the road as a still header. Optional, like
// all of the page's script: without it the road is a gradient, the Omnibar's rows are links to
// their cards, and the introduction is an ordinary video.

import { crtRoad } from "./crt-road.js";
import { SceneHost } from "./scene.js";

const drive = document.querySelector(".drive");
if (drive) {
  const smooth = !matchMedia("(prefers-reduced-motion: reduce)").matches;
  const typing = (event) => event.target.closest("input, textarea, select, [contenteditable]");
  const host = new SceneHost(drive.querySelector(".drive__scene canvas"), crtRoad);

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
  const paint = (next) => {
    theme = next;
    document.documentElement.dataset.theme = theme;
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

  // The browser's own keys: o to the Omnibar, T to the next theme.
  addEventListener("keydown", (event) => {
    if (typing(event) || event.ctrlKey || event.metaKey || event.altKey) return;
    if (event.key === "o") {
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
if (header) new SceneHost(header, crtRoad, { held: true });
