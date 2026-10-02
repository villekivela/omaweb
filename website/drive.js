// The landing page's behaviour: the road behind it, the Omnibar on the Start page, the
// introduction and the theme; and on a release page, the road as a still header. Optional, like
// all of the page's script: without it the road is a gradient, the Omnibar's rows are links to
// their cards, and the introduction is an ordinary video.

import { createCrtRoad } from "./crt-road.js";
import { filmKey, filmState, seekAt } from "./film.js";
import { SceneHost } from "./scene.js";

// The road's parameters, shared with the browser's own road and served beside this script. A
// page that cannot fetch them keeps the gradient the road would have stood on.
const crtRoad = await fetch(new URL("crt-road.json", import.meta.url))
  .then((response) => (response.ok ? response.json() : Promise.reject(response.status)))
  .then(createCrtRoad)
  .catch(() => null);

// The radio's beat, 0 to 1, read for each frame of the road: how far the song's bass rises over
// its own recent level, struck at once and let fall. Nothing until the radio hands over its
// analyser, and nothing while it is paused.
function listenToRadio() {
  const radio = document.querySelector(".radio");
  let analyser = null;
  let samples = null;
  let level = 0;
  let beat = 0;
  let then = 0;
  radio?.addEventListener("listen", (event) => {
    analyser = event.detail;
    samples = new Float32Array(analyser.fftSize);
  });
  return () => {
    if (!analyser || radio.paused) return 0;
    const now = performance.now();
    // The host may ask more than once a frame; the beat moves on only with time.
    if (now - then < 16) return beat;
    const step = Math.min(0.1, (now - then) / 1000);
    then = now;
    analyser.getFloatTimeDomainData(samples);
    let sum = 0;
    for (const sample of samples) sum += sample * sample;
    const bass = Math.sqrt(sum / samples.length);
    level += (bass - level) * Math.min(1, step * 2);
    const hit = level > 1e-4 ? Math.min(1, Math.max(0, (bass / level - 1) * 1.5)) : 0;
    beat = Math.max(hit, beat * Math.exp(-step * 8));
    return beat;
  };
}

const drive = document.querySelector(".drive");
if (drive) {
  const smooth = !matchMedia("(prefers-reduced-motion: reduce)").matches;
  const typing = (event) => event.target.closest("input, textarea, select, [contenteditable]");
  const canvas = drive.querySelector(".drive__scene canvas");
  const omnibar = drive.querySelector(".omnibar");
  // The Omnibar stands in front of the sun and catches its light at its rim.
  const host = crtRoad
    ? new SceneHost(canvas, crtRoad, { beat: listenToRadio(), lit: omnibar })
    : { setNavigating() {} };

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

  // The introduction plays muted while it is on screen and stops when it leaves, unless the reader
  // paused it, which holds until they play it again. Under reduced motion it waits on its poster
  // for the reader to press play. Its own controls stand in for the browser's.
  const film = drive.querySelector(".film__video");
  if (film) {
    film.removeAttribute("controls");
    const frame = film.closest(".film__frame");
    const controls = frame.querySelector(".film__controls");
    const toggle = controls.querySelector(".film__toggle");
    const scrubber = controls.querySelector(".film__scrubber");
    const elapsed = controls.querySelector(".film__elapsed");
    const total = controls.querySelector(".film__total");
    let held = false;
    const play = () => {
      held = false;
      film.play().catch(() => {});
    };
    const pause = () => {
      held = true;
      film.pause();
    };
    const flip = () => (film.paused ? play() : pause());
    const show = () => {
      const state = filmState(film);
      toggle.setAttribute("aria-pressed", String(state.playing));
      frame.toggleAttribute("data-paused", !state.playing);
      scrubber.setAttribute("aria-valuemax", String(state.valueMax));
      scrubber.setAttribute("aria-valuenow", String(state.valueNow));
      scrubber.setAttribute("aria-valuetext", state.valueText);
      scrubber.style.setProperty("--progress", String(state.progress));
      elapsed.textContent = state.elapsed;
      total.textContent = state.total;
    };
    for (const type of ["play", "pause", "timeupdate", "seeked", "durationchange"]) {
      film.addEventListener(type, show);
    }
    show();

    // Idle, the controls get out of the way; a pointer moving over the film, or a tap on a touch
    // screen, brings them back for a while. Hovering or focusing them keeps them, as does a pause.
    let resting = 0;
    const wake = () => {
      frame.setAttribute("data-awake", "");
      clearTimeout(resting);
      resting = setTimeout(() => frame.removeAttribute("data-awake"), 2500);
    };
    frame.addEventListener("pointermove", (event) => {
      if (event.pointerType === "mouse") wake();
    });
    film.addEventListener("click", (event) => {
      // A first tap on a touch screen only shows the controls; a click plays or pauses.
      if (event.pointerType === "touch" && !frame.hasAttribute("data-awake")) wake();
      else {
        flip();
        wake();
      }
    });
    toggle.addEventListener("click", flip);

    // A press on the scrubber seeks there, and a drag carries on while the pointer is held.
    const seek = (event) => {
      film.currentTime = seekAt(event.clientX, scrubber.getBoundingClientRect(), film.duration);
    };
    scrubber.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      scrubber.setPointerCapture(event.pointerId);
      scrubber.focus({ preventScroll: true });
      seek(event);
    });
    scrubber.addEventListener("pointermove", (event) => {
      if (scrubber.hasPointerCapture(event.pointerId)) seek(event);
    });

    // The keys are the film's while its controls have focus, before the page's own J, K and f.
    controls.addEventListener("keydown", (event) => {
      if (event.ctrlKey || event.metaKey || event.altKey) return;
      const action = filmKey(event.key, film);
      if (!action || (action.seek !== undefined && event.target !== scrubber)) return;
      event.preventDefault();
      event.stopPropagation();
      wake();
      if (action.toggle) flip();
      else film.currentTime = action.seek;
    });
    // Space would press the focused button once more as it rises.
    controls.addEventListener("keyup", (event) => {
      if (event.key === " ") event.preventDefault();
    });

    if (smooth) {
      controls.hidden = false;
      new IntersectionObserver(
        ([entry]) => {
          if (!entry.isIntersecting) film.pause();
          else if (!held) film.play().catch(() => {});
        },
        { threshold: 0.5 },
      ).observe(film);
    } else {
      const start = drive.querySelector(".film__play");
      start.hidden = false;
      start.addEventListener("click", () => {
        start.hidden = true;
        controls.hidden = false;
        play();
        toggle.focus({ preventScroll: true });
      });
    }
  }

  // The Omnibar: type to filter the features, arrows to choose a row, Return to go to its card.
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
