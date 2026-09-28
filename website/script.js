// Optional behaviour. Without it the page is complete: the menu stays open as a row of links and
// the dashboard shows its address already typed.

const menu = document.querySelector(".menu");
if (menu) {
  menu.hidden = false;
  menu.addEventListener("click", () => {
    const open = menu.getAttribute("aria-expanded") === "true";
    menu.setAttribute("aria-expanded", String(!open));
  });
}

// The dashboard screen types a line one key at a time, the way a reader would, holds it, erases
// it and moves on to the next. Every line fits the screen, which holds about eighteen characters.
const screen = document.querySelector("[data-type]");
const still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
const lines = [
  "omaweb.app",
  "localhost:3000",
  "no mouse detected",
  "telemetry: 0 bytes",
  "space: work",
  "j j j k",
  "gg",
  ":split view",
  "wiki.archlinux.org",
  "keybindings.json",
  "104 keys ready",
  "localhost:5173",
];

const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function drive() {
  for (let index = 0; ; index = (index + 1) % lines.length) {
    const line = lines[index];
    for (let shown = 1; shown <= line.length; shown++) {
      screen.textContent = line.slice(0, shown);
      await pause(70 + Math.random() * 90);
    }
    await pause(2600);
    for (let shown = line.length - 1; shown >= 0; shown--) {
      screen.textContent = line.slice(0, shown);
      await pause(28);
    }
    await pause(450);
  }
}

if (screen && !still) {
  screen.textContent = "";
  pause(700).then(drive);
}

// The walkthrough: one window, one shot at a time. Whichever step is in the middle of the screen
// picks the shot, and the swatches pick the theme, which only the window takes. While the section
// is on screen it answers the browser's own keys by scrolling to the step they belong to, so the
// page shows what the keyboard does rather than describing it.
const shot = document.querySelector("[data-shot]");
const steps = [...document.querySelectorAll("[data-step]")];
const themePicker = document.querySelector(".themes");
if (shot && steps.length && themePicker) {
  const image = shot.querySelector("img");
  const open = shot.querySelector(".shot__open");
  const swatches = [...themePicker.querySelectorAll("[data-theme-choice]")];
  const themeName = themePicker.querySelector(".themes__name");
  const viewer = document.querySelector(".viewer");
  let state = steps[0].dataset.step;
  let theme = shot.dataset.theme;
  let onScreen = false;

  const source = (forState, forTheme) => `assets/shots/${forTheme}/${forState}.webp`;
  const prefetch = (forState, forTheme) => {
    new Image().src = source(forState, forTheme);
  };

  const show = (nextState, nextTheme) => {
    if (nextState === state && nextTheme === theme) return;
    state = nextState;
    theme = nextTheme;
    const address = source(state, theme);
    image.src = address;
    open.href = address;
    shot.dataset.theme = theme;
    for (const step of steps) {
      const on = step.dataset.step === state;
      step.classList.toggle("is-on", on);
      if (on) image.alt = step.dataset.alt;
    }
    for (const swatch of swatches) {
      const on = swatch.dataset.themeChoice === theme;
      swatch.setAttribute("aria-pressed", String(on));
      if (on) themeName.textContent = swatch.dataset.name;
    }
  };

  // The step whose box crosses the reading line is the one on show. Side by side that is the
  // middle of the screen; stacked, the window covers the top of the screen, so the line is the
  // middle of what is left under it.
  const stage = document.querySelector(".walk__stage");
  const stacked = window.matchMedia("(max-width: 820px)");
  let middle = null;
  const watch = () => {
    middle?.disconnect();
    const covered = stacked.matches ? stage.offsetHeight : 0;
    const line = (covered + (innerHeight - covered) / 2) / innerHeight;
    middle = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) show(entry.target.dataset.step, theme);
        }
      },
      { rootMargin: `-${(line * 100).toFixed(1)}% 0px -${((1 - line) * 100).toFixed(1)}% 0px` },
    );
    for (const step of steps) middle.observe(step);
  };
  watch();
  // The stage centres itself on the reading line, so the stylesheet needs its real height, which
  // the shot sets once it loads and the width changes after.
  new ResizeObserver(([entry]) => {
    stage.style.setProperty("--stage-height", `${entry.borderBoxSize[0].blockSize}px`);
  }).observe(stage);
  let resizing = 0;
  window.addEventListener("resize", () => {
    clearTimeout(resizing);
    resizing = setTimeout(watch, 150);
  });
  for (const step of steps) {
    // The next step's shot, fetched while this one is read.
    const index = steps.indexOf(step);
    if (steps[index + 1]) {
      new IntersectionObserver(([entry], observer) => {
        if (!entry.isIntersecting) return;
        prefetch(steps[index + 1].dataset.step, theme);
        observer.disconnect();
      }).observe(step);
    }
  }

  new IntersectionObserver(
    (entries) => {
      onScreen = entries.some((entry) => entry.isIntersecting);
    },
    { threshold: 0.1 },
  ).observe(document.querySelector(".walk"));

  for (const swatch of swatches) {
    swatch.addEventListener("click", () => show(state, swatch.dataset.themeChoice));
    swatch.addEventListener("pointerenter", () => prefetch(state, swatch.dataset.themeChoice));
  }

  const goTo = (target) => {
    const step = steps.find((candidate) => candidate.dataset.step === target);
    step?.scrollIntoView({
      behavior: still ? "auto" : "smooth",
      block: stacked.matches ? "end" : "center",
    });
    show(target, theme);
  };

  // The same chords Omaweb binds: Ctrl+B the sidebar, Ctrl+Y History, Ctrl+, Settings, and
  // Escape back to the page. A second press of a chord goes back, as the browser's toggle does.
  const chords = { b: "collapsed", y: "history", ",": "settings" };
  document.addEventListener("keydown", (event) => {
    if (!onScreen || viewer?.open) return;
    if (event.target.closest("input, textarea, select, [contenteditable]")) return;
    const key = event.key.toLowerCase();
    if ((event.ctrlKey || event.metaKey) && !event.altKey && chords[key]) {
      event.preventDefault();
      goTo(state === chords[key] ? "space" : chords[key]);
    } else if (key === "escape" && state !== "space") {
      goTo("space");
    } else if (key === "t" && !event.ctrlKey && !event.metaKey && !event.altKey) {
      const index = swatches.findIndex((swatch) => swatch.dataset.themeChoice === theme);
      show(state, swatches[(index + 1) % swatches.length].dataset.themeChoice);
    }
  });

  // The window opens full size over the page rather than leaving it.
  if (viewer?.showModal) {
    const full = viewer.querySelector(".viewer__image");
    open.addEventListener("click", (event) => {
      event.preventDefault();
      full.src = image.currentSrc || image.src;
      full.alt = image.alt;
      viewer.showModal();
    });
    viewer.querySelector(".viewer__close").addEventListener("click", () => viewer.close());
    // A click on the backdrop lands on the dialog itself, outside the image.
    viewer.addEventListener("click", (event) => {
      if (event.target === viewer) viewer.close();
    });
  }

  themePicker.hidden = false;
  document.querySelector(".step__hint")?.removeAttribute("hidden");
}

// The night radio, on the hidden switch on the dashboard or the M key: each press tunes to the
// next station, then off. The screen on the dash glows with it.
const radioSwitch = document.querySelector("[data-radio]");
if (radioSwitch && window.OmawebRadio && window.AudioContext) {
  const radio = window.OmawebRadio();
  const dash = document.querySelector(".dash");
  const glow = () => {
    if (!radio.playing) {
      dash?.style.setProperty("--level", "0");
      return;
    }
    dash?.style.setProperty("--level", radio.level().toFixed(3));
    requestAnimationFrame(glow);
  };
  // Each press tunes to the next station, and the last one's press turns the radio off.
  const toggle = () => {
    const wasPlaying = radio.playing;
    const name = radio.tune();
    radioSwitch.setAttribute("aria-pressed", String(radio.playing));
    radioSwitch.setAttribute("aria-label", name ? `Night radio: ${name}` : "Night radio");
    if (radio.playing && !wasPlaying && !still) glow();
  };
  radioSwitch.addEventListener("click", toggle);
  document.addEventListener("keydown", (event) => {
    if (event.key.toLowerCase() !== "m" || event.ctrlKey || event.metaKey || event.altKey) return;
    if (event.target.closest("input, textarea, select, [contenteditable]")) return;
    toggle();
  });
  radioSwitch.hidden = false;
}

// Copies a screen's commands without the prompts, which are drawn and not typed.
for (const button of document.querySelectorAll("[data-copy]")) {
  if (!navigator.clipboard) break;
  const code = button.closest(".hud").querySelector("code");
  button.addEventListener("click", async () => {
    const lines = [...code.childNodes]
      .filter(
        (node) => !node.classList?.contains("hud__prompt") && !node.classList?.contains("caret"),
      )
      .map((node) => node.textContent)
      .join("");
    try {
      await navigator.clipboard.writeText(lines.trim());
      button.textContent = "Copied";
      button.classList.add("is-done");
    } catch {
      button.textContent = "Select to copy";
    }
    setTimeout(() => {
      button.textContent = "Copy";
      button.classList.remove("is-done");
    }, 1800);
  });
  button.hidden = false;
}

// The release log: the filter by kind, and the list opened at the release being read. Without the
// script every release is listed and the filter stays hidden.
const log = document.querySelector(".log__versions");
if (log) {
  const rows = [...log.querySelectorAll("[data-kind]")];
  const choices = [...log.querySelectorAll("[data-kind-choice]")];
  const filter = (kind) => {
    for (const row of rows) row.hidden = kind !== "all" && row.dataset.kind !== kind;
    log.querySelector(".log__list")?.classList.toggle("is-one-kind", kind !== "all");
    for (const choice of choices) {
      choice.setAttribute("aria-pressed", String(choice.dataset.kindChoice === kind));
    }
  };
  for (const choice of choices) {
    choice.addEventListener("click", () => filter(choice.dataset.kindChoice));
  }
  const pressed = choices.find((choice) => choice.getAttribute("aria-pressed") === "true");
  filter(pressed ? pressed.dataset.kindChoice : "all");
  log.querySelector(".log__kinds").hidden = false;
  const current = log.querySelector('[aria-current="page"]');
  const screen = log.querySelector(".log__screen");
  if (current && screen) {
    screen.scrollTop = current.offsetTop - screen.clientHeight / 2 + current.offsetHeight / 2;
  }
}
