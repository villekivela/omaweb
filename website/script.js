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

// The walkthrough: a reel of the window's views, one card each. The bar under it pages the reel
// and picks the theme, which every card's frame takes at once. While the section is on screen it
// answers the browser's own keys by bringing their card into view, so the page shows what the
// keyboard does rather than describing it.
const reel = document.querySelector(".reel");
if (reel) {
  const track = reel.querySelector(".reel__track");
  const cards = [...track.querySelectorAll(".reel__card")];
  const bar = reel.querySelector(".reel__bar");
  const picker = bar.querySelector(".themes");
  const swatches = [...picker.querySelectorAll("[data-theme-choice]")];
  const themeName = picker.querySelector(".themes__name");
  const count = bar.querySelector(".reel__count");
  const [back, forward] = bar.querySelectorAll(".reel__step");
  const viewer = document.querySelector(".viewer");
  let theme = cards[0].querySelector(".shot").dataset.theme;
  let onScreen = false;

  const source = (card, forTheme) => `assets/shots/${forTheme}/${card.dataset.step}.webp`;
  const prefetch = (card, forTheme) => {
    new Image().src = source(card, forTheme);
  };

  // The card at the reel's left edge, or the last one once the reel can scroll no further.
  const at = () => {
    if (track.scrollLeft >= track.scrollWidth - track.clientWidth - 2) return cards.length - 1;
    const pitch = cards[1].offsetLeft - cards[0].offsetLeft;
    return Math.round(track.scrollLeft / pitch);
  };

  const mark = () => {
    const index = at();
    count.textContent = `${index + 1} / ${cards.length}`;
    back.disabled = index === 0;
    forward.disabled = index === cards.length - 1;
  };

  const goTo = (index) => {
    const card = cards[Math.max(0, Math.min(cards.length - 1, index))];
    track.scrollTo({
      left: card.offsetLeft - cards[0].offsetLeft,
      behavior: still ? "auto" : "smooth",
    });
  };

  const paint = (nextTheme) => {
    theme = nextTheme;
    for (const card of cards) {
      const address = source(card, theme);
      card.querySelector(".shot").dataset.theme = theme;
      card.querySelector("img").src = address;
      card.querySelector(".shot__open").href = address;
    }
    for (const swatch of swatches) {
      const on = swatch.dataset.themeChoice === theme;
      swatch.setAttribute("aria-pressed", String(on));
      if (on) themeName.textContent = swatch.dataset.name;
    }
  };

  track.addEventListener("scroll", mark, { passive: true });
  for (const button of [back, forward]) {
    button.addEventListener("click", () => goTo(at() + Number(button.dataset.stepBy)));
  }
  for (const swatch of swatches) {
    swatch.addEventListener("click", () => paint(swatch.dataset.themeChoice));
    swatch.addEventListener("pointerenter", () =>
      prefetch(cards[at()], swatch.dataset.themeChoice),
    );
  }

  new IntersectionObserver(
    (entries) => {
      onScreen = entries.some((entry) => entry.isIntersecting);
    },
    { threshold: 0.1 },
  ).observe(reel);

  // The same keys Omaweb binds: o the Omnibar, Ctrl+B the sidebar, and Escape back to the page. A
  // second press of Ctrl+B goes back, as the browser's toggle does. Ctrl+L, the Omnibar's other
  // key, stays the reader's own browser's.
  const chords = { b: "collapsed" };
  const indexOf = (step) => cards.findIndex((card) => card.dataset.step === step);
  document.addEventListener("keydown", (event) => {
    if (!onScreen || viewer?.open) return;
    if (event.target.closest("input, textarea, select, [contenteditable]")) return;
    const key = event.key.toLowerCase();
    const current = cards[at()].dataset.step;
    if ((event.ctrlKey || event.metaKey) && !event.altKey && chords[key]) {
      event.preventDefault();
      goTo(indexOf(current === chords[key] ? "space" : chords[key]));
    } else if (key === "escape" && current !== "space") {
      goTo(indexOf("space"));
    } else if (event.ctrlKey || event.metaKey || event.altKey) {
      return;
    } else if (key === "o") {
      goTo(indexOf("omnibar"));
    } else if (key === "t") {
      const index = swatches.findIndex((swatch) => swatch.dataset.themeChoice === theme);
      paint(swatches[(index + 1) % swatches.length].dataset.themeChoice);
    }
  });

  // A window opens full size over the page rather than leaving it.
  if (viewer?.showModal) {
    const full = viewer.querySelector(".viewer__image");
    for (const card of cards) {
      card.querySelector(".shot__open").addEventListener("click", (event) => {
        event.preventDefault();
        const image = card.querySelector("img");
        full.src = image.currentSrc || image.src;
        full.alt = image.alt;
        viewer.showModal();
      });
    }
    viewer.querySelector(".viewer__close").addEventListener("click", () => viewer.close());
    // A click on the backdrop lands on the dialog itself, outside the image.
    viewer.addEventListener("click", (event) => {
      if (event.target === viewer) viewer.close();
    });
  }

  mark();
  bar.hidden = false;
  picker.hidden = false;
  reel.querySelector(".reel__hint")?.removeAttribute("hidden");
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
