// Optional behaviour shared by the landing page and the release pages. Without it the page is
// complete: the menu stays open as a row of links. The landing page's own is drive.js.

const menu = document.querySelector(".menu");
if (menu) {
  menu.hidden = false;
  menu.addEventListener("click", () => {
    const open = menu.getAttribute("aria-expanded") === "true";
    menu.setAttribute("aria-expanded", String(!open));
  });
}

// The night radio, on a Radio button and the M key: one song, Night road, on a loop,
// and a second press stops it. Nothing loads or plays until a reader asks for it.
//
// On the first press the song is also routed through Web Audio, to an analyser of its bass that
// the radio hands out as a `listen` event, so the landing page's sun can pulse with the beat. The
// audio context is made in the press itself, since a browser only lets one start from a reader's
// gesture, and a context that never started would silence the song.
const radio = document.querySelector(".radio");
const radioToggles = document.querySelectorAll(".radio-toggle");
if (radio) {
  let context = null;
  const listen = () => {
    if (!window.AudioContext) return;
    if (!context) {
      context = new AudioContext();
      const song = context.createMediaElementSource(radio);
      song.connect(context.destination);
      const bass = new BiquadFilterNode(context, { type: "lowpass", frequency: 150 });
      const analyser = new AnalyserNode(context, { fftSize: 1024 });
      song.connect(bass).connect(analyser);
      radio.dispatchEvent(new CustomEvent("listen", { detail: analyser }));
    }
    context.resume().catch(() => {});
  };
  const toggle = () => {
    if (!radio.paused) return radio.pause();
    listen();
    radio.play().catch(() => {});
  };
  const show = () => {
    for (const button of radioToggles) button.setAttribute("aria-pressed", String(!radio.paused));
  };
  radio.addEventListener("play", show);
  radio.addEventListener("pause", show);
  for (const button of radioToggles) {
    button.addEventListener("click", toggle);
    button.hidden = false;
  }
  document.addEventListener("keydown", (event) => {
    if (event.key.toLowerCase() !== "m" || event.ctrlKey || event.metaKey || event.altKey) return;
    if (event.target.closest("input, textarea, select, [contenteditable]")) return;
    toggle();
  });
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
