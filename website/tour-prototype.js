// PROTOTYPE, throwaway (branch feat/ignite-tour-prototype): the Ignite tour. Ignite plays the
// night radio and drives the page through its stops on the song's cues; anything the reader does
// takes the wheel back and leaves the music on, and Resume drive carries on from the stop nearest
// to where they are. Two ways to drive, on ?drive=: "scroll", the page scrolling from stop to stop
// as it looks today, and "road", the scrim lifting and the road racing on a slower glide between
// stops. ?clock=timer drives on a timer, as it does when the radio cannot play.

// Night road runs at about 107.5 beats a minute, so a bar is 2.23 seconds; the beat comes in after
// eight bars and the sections turn every eight. Each card holds four bars. Tune by ear: the
// prototype bar shows the song's time.
const BAR = 240 / 107.5;
const STOPS = [
  { name: "Start", bars: 0, target: ".start", top: true, perform: typeQuery },
  { name: "In use", bars: 8, target: ".film", perform: playFilm },
  { name: "One field for everything", bars: 16, target: "#omnibar" },
  { name: "Work and personal, kept apart", bars: 20, target: "#spaces" },
  { name: "Keyboard first", bars: 24, target: "#keyboard", perform: pressKeys },
  { name: "Follows your Omarchy theme", bars: 28, target: "#theme", perform: cycleThemes },
  { name: "Private by default", bars: 32, target: "#private" },
  { name: "Blocking, built in", bars: 36, target: "#blocking" },
  { name: "Split view and docked DevTools", bars: 40, target: "#pros" },
  { name: "Agents drive their own Spaces", bars: 44, target: "#agents" },
  { name: "Get on the road", bars: 48, target: "#install" },
].map((stop) => ({ ...stop, cue: stop.bars * BAR }));

const params = new URLSearchParams(location.search);
const VARIANTS = ["scroll", "road"];
const variant = VARIANTS.includes(params.get("drive")) ? params.get("drive") : "scroll";
const forceTimer = params.get("clock") === "timer";

// What a stop does while it is the current one; each returns how to undo it.
function later(timers, seconds, action) {
  timers.push(setTimeout(action, seconds * 1000));
}

// The Start page: the Omnibar types a query, its rows narrowing to the feature, then clears.
function typeQuery() {
  const input = document.querySelector(".omnibar__input");
  const timers = [];
  const set = (value) => {
    input.value = value;
    input.dispatchEvent(new Event("input"));
  };
  const query = "keyboard";
  [...query].forEach((_, at) => later(timers, 2 + at * 0.18, () => set(query.slice(0, at + 1))));
  later(timers, 2 + 4 * BAR, () => set(""));
  return () => {
    timers.forEach(clearTimeout);
    set("");
  };
}

// The film plays from its start; it is silent, so it runs under the song.
function playFilm() {
  const film = document.querySelector(".film__video");
  if (film) {
    film.currentTime = 0;
    film.play().catch(() => {});
  }
  return () => {};
}

// Keyboard first: its keys light one by one, then the page's own link hints show on f.
function pressKeys(card) {
  const timers = [];
  const keys = [...card.querySelectorAll("kbd")];
  keys.forEach((key, at) => later(timers, 0.6 + at * 0.4, () => key.classList.add("tour-key")));
  const press = (key) =>
    document.body.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true }));
  let hinting = false;
  later(timers, BAR, () => {
    hinting = true;
    press("f");
  });
  later(timers, 3 * BAR, () => {
    hinting = false;
    press("Escape");
  });
  return () => {
    timers.forEach(clearTimeout);
    keys.forEach((key) => key.classList.remove("tour-key"));
    if (hinting) press("Escape");
  };
}

// The theme card: the page wears the next three themes, one a bar, and returns to its own.
function cycleThemes() {
  const timers = [];
  const swatches = [...document.querySelectorAll("[data-theme-choice]")];
  const own = document.documentElement.dataset.theme;
  const at = swatches.findIndex((swatch) => swatch.dataset.themeChoice === own);
  const pick = (index) => swatches[(index + swatches.length) % swatches.length]?.click();
  for (let step = 1; step <= 3; ++step)
    later(timers, (step - 1) * BAR + 0.8, () => pick(at + step));
  later(timers, 3 * BAR + 0.8, () => pick(at));
  return () => {
    timers.forEach(clearTimeout);
    if (document.documentElement.dataset.theme !== own) pick(at);
  };
}

export function igniteTourPrototype({ drive, host, smooth }) {
  const radio = document.querySelector(".radio");
  const radioToggle = document.querySelector(".radio-toggle");
  const below = drive.querySelector(".start__below");
  if (!radio || !radioToggle || !below) return;

  // The road's speed: during a glide the "road" variant asks for all of it.
  let boost = 0;
  const setNavigating = host.setNavigating.bind(host);
  host.setNavigating = (value) => setNavigating(Math.max(value, boost));

  const ignite = document.createElement("button");
  ignite.className = "btn ignite";
  ignite.type = "button";
  ignite.innerHTML = '<span aria-hidden="true">&#9654;</span> Ignite';
  below.prepend(ignite);

  const chip = document.createElement("div");
  chip.className = "tour-chip";
  chip.hidden = true;
  chip.innerHTML = `
    <div class="tour-chip__text">
      <span class="tour-chip__name"></span>
      <span class="tour-chip__line"><i></i></span>
    </div>
    <button class="tour-chip__resume" type="button">Resume drive</button>
    <button class="tour-chip__stop" type="button">Stop <kbd>Esc</kbd></button>`;
  document.body.append(chip);
  const chipName = chip.querySelector(".tour-chip__name");
  const chipLine = chip.querySelector(".tour-chip__line i");
  const resumeButton = chip.querySelector(".tour-chip__resume");

  const bar = document.createElement("div");
  bar.className = "proto-bar";
  bar.innerHTML = `
    <button type="button" data-by="-1" aria-label="Previous variant">&larr;</button>
    <span class="proto-bar__variant"></span>
    <button type="button" data-by="1" aria-label="Next variant">&rarr;</button>
    <span class="proto-bar__state"></span>`;
  document.body.append(bar);
  bar.querySelector(".proto-bar__variant").textContent = `drive=${variant}`;
  const barState = bar.querySelector(".proto-bar__state");
  const switchVariant = (by) => {
    const next = VARIANTS[(VARIANTS.indexOf(variant) + by + VARIANTS.length) % VARIANTS.length];
    params.set("drive", next);
    location.search = params.toString();
  };
  for (const button of bar.querySelectorAll("[data-by]")) {
    button.addEventListener("click", () => switchVariant(Number(button.dataset.by)));
  }

  // off, driving, held (the reader has the wheel, the music plays on) or parked (at Install,
  // the song playing out).
  let state = "off";
  let current = -1;
  let undo = () => {};
  let timerMode = false;
  let timerStart = 0;
  let glide = 0;
  const clock = () =>
    timerMode ? (performance.now() - timerStart) / 1000 : radio.currentTime || 0;
  const seek = (seconds) => {
    if (timerMode) timerStart = performance.now() - seconds * 1000;
    else radio.currentTime = seconds;
  };

  const clearStop = () => {
    undo();
    undo = () => {};
    for (const element of drive.querySelectorAll("[data-tour-current]")) {
      element.removeAttribute("data-tour-current");
    }
  };

  // Where a stop sits: the Start page at the top, a stop shorter than the window in its middle,
  // a taller one at its top.
  const targetY = (element, top) => {
    if (top) return 0;
    const rect = element.getBoundingClientRect();
    const y =
      rect.height < innerHeight
        ? rect.top + scrollY - (innerHeight - rect.height) / 2
        : rect.top + scrollY - 24;
    return Math.max(0, Math.min(document.documentElement.scrollHeight - innerHeight, y));
  };

  const travel = (y) => {
    cancelAnimationFrame(glide);
    if (!smooth) return scrollTo({ top: y, behavior: "auto" });
    if (variant === "scroll") return scrollTo({ top: y, behavior: "smooth" });
    // The road: a slow glide with the scrim lifted and the road at full speed.
    const from = scrollY;
    const length = 1800;
    const began = performance.now();
    drive.setAttribute("data-tour-glide", "");
    boost = 1;
    const step = (now) => {
      const t = Math.min(1, (now - began) / length);
      const eased = t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) ** 3 / 2;
      scrollTo({ top: from + (y - from) * eased, behavior: "auto" });
      if (t < 1) glide = requestAnimationFrame(step);
      else {
        glide = 0;
        boost = 0;
        host.setNavigating(0);
        drive.removeAttribute("data-tour-glide");
      }
    };
    glide = requestAnimationFrame(step);
  };

  const goTo = (index) => {
    clearStop();
    current = index;
    const stop = STOPS[index];
    const element = drive.querySelector(stop.target);
    if (!stop.top) element.setAttribute("data-tour-current", "");
    travel(targetY(element, stop.top));
    undo = stop.perform?.(element) || (() => {});
  };

  const show = () => {
    chip.hidden = state === "off";
    chip.dataset.state = state;
    resumeButton.hidden = state !== "held";
    const stop = STOPS[current];
    chipName.textContent =
      state === "held" ? "You have the wheel" : state === "parked" ? "End of the road" : stop?.name;
    const end = STOPS[STOPS.length - 1].cue;
    chipLine.style.width = `${Math.min(100, (clock() / end) * 100)}%`;
    const time = clock();
    barState.textContent = `${timerMode ? "timer" : "song"} ${Math.floor(time / 60)}:${String(
      Math.floor(time % 60),
    ).padStart(
      2,
      "0",
    )}.${Math.floor((time % 1) * 10)} · ${state} · stop ${current + 1}/${STOPS.length}`;
  };

  const tick = () => {
    if (state === "driving") {
      const time = clock();
      let index = 0;
      while (index + 1 < STOPS.length && STOPS[index + 1].cue <= time) ++index;
      if (index !== current) goTo(index);
      if (index === STOPS.length - 1) {
        state = "parked";
        if (timerMode) setTimeout(() => state === "parked" && end(), 4000);
      }
    }
    show();
  };
  setInterval(tick, 100);

  const end = () => {
    state = "off";
    clearStop();
    cancelAnimationFrame(glide);
    boost = 0;
    drive.removeAttribute("data-tour-glide");
    drive.removeAttribute("data-tour");
    radio.loop = true;
    show();
  };

  const start = () => {
    clearStop();
    current = -1;
    state = "driving";
    drive.setAttribute("data-tour", "");
    timerMode = forceTimer;
    timerStart = performance.now();
    radio.loop = false;
    // The radio's own toggle, from inside the press, so its analyser starts and the sun beats.
    if (radio.paused) radioToggle.click();
    radio.currentTime = 0;
    // A radio that does not play, as when autoplay is refused, leaves the drive to the timer.
    setTimeout(() => {
      if (state !== "off" && !timerMode && (radio.paused || radio.muted)) {
        timerMode = true;
        timerStart = performance.now() - (radio.currentTime || 0) * 1000;
      }
    }, 1500);
    tick();
  };

  // The reader takes the wheel: the drive lets go and the music plays on, on its loop again.
  const hold = () => {
    if (state !== "driving") return;
    state = "held";
    clearStop();
    cancelAnimationFrame(glide);
    boost = 0;
    drive.removeAttribute("data-tour-glide");
    radio.loop = true;
    show();
  };

  // Resume from the stop nearest the middle of the window, the song sought to its cue.
  const resume = () => {
    const middle = innerHeight / 2;
    const distance = (stop) => {
      const rect = drive.querySelector(stop.target).getBoundingClientRect();
      return Math.abs(rect.top + Math.min(rect.height, innerHeight) / 2 - middle);
    };
    let nearest = 0;
    STOPS.forEach((stop, index) => {
      if (distance(stop) < distance(STOPS[nearest])) nearest = index;
    });
    radio.loop = false;
    seek(STOPS[nearest].cue + 0.01);
    current = -1;
    state = "driving";
    if (!timerMode && radio.paused) radio.play().catch(() => {});
    tick();
  };

  const stopAll = () => {
    end();
    if (!radio.paused) radio.pause();
  };

  ignite.addEventListener("click", start);
  resumeButton.addEventListener("click", resume);
  chip.querySelector(".tour-chip__stop").addEventListener("click", stopAll);

  // The radio stopped by the reader, on Radio or M, ends the drive; the song's end ends a parked
  // one.
  radio.addEventListener("pause", () => {
    if (state !== "off" && !radio.ended) end();
  });
  radio.addEventListener("ended", () => state !== "off" && end());

  const ours = (target) =>
    target instanceof Element && target.closest(".ignite, .tour-chip, .proto-bar");
  for (const type of ["wheel", "touchstart", "pointerdown"]) {
    addEventListener(
      type,
      (event) => {
        if (event.isTrusted && !ours(event.target)) hold();
      },
      { capture: true, passive: true },
    );
  }
  addEventListener(
    "keydown",
    (event) => {
      if (!event.isTrusted || ours(event.target)) return;
      if (event.key === "Escape" && state !== "off") {
        event.stopImmediatePropagation();
        return stopAll();
      }
      // The prototype's own keys: the arrows switch the variant.
      if (
        (event.key === "ArrowLeft" || event.key === "ArrowRight") &&
        !event.target.closest?.("input")
      ) {
        return switchVariant(event.key === "ArrowLeft" ? -1 : 1);
      }
      if (event.key.toLowerCase() !== "m") hold();
    },
    { capture: true },
  );
  show();
}
