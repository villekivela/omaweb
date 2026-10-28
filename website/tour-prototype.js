// PROTOTYPE, throwaway (branch feat/ignite-tour-prototype): driving through the site. On a wide
// screen that allows motion, the page is a road: scroll is the throttle, and each stop (the Start
// page, the film, every feature, the install) rises from the horizon, holds while it is read and
// passes the camera. Ignite, at the end of the Omnibar, is cruise control: the same drive steered
// by Night road's cues. Anything the reader does takes the wheel back and leaves the music on.
// ?clock=timer drives on a timer, as it does when the radio cannot play. On a phone or under
// reduced motion the page stays stacked, with the streamlined Omnibar and no drive.

import { STILL_MEDIA } from "./scene.js";

// Night road runs at about 107.5 beats a minute, so a bar is 2.23 seconds; the beat comes in after
// eight bars and the sections turn every eight. Each card holds four bars. Tune by ear: the
// prototype bar shows the song's time.
const BAR = 240 / 107.5;
const STOPS = [
  { name: "Start", bars: 0, target: ".start", perform: typeQuery },
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

const forceTimer = new URLSearchParams(location.search).get("clock") === "timer";

// Where a stop is on the road, by `t`, its distance in stops from the one being read: beyond -1
// it is past the horizon, it approaches to -0.3, holds to 0.3 and passes the camera by 0.85.
const FAR = -1;
const ARRIVE = -0.3;
const LEAVE = 0.3;
const GONE = 0.85;
const HORIZON_DEPTH = 12;

function placement(t, last) {
  if (last) t = Math.min(t, 0);
  if (t <= FAR || t >= GONE) return null;
  if (t < ARRIVE) {
    const near = (t - FAR) / (ARRIVE - FAR);
    const depth = 1 + (1 - near) * (HORIZON_DEPTH - 1);
    return { scale: 1 / depth, opacity: Math.min(1, near * 2.5), depth, reading: false, drop: 0 };
  }
  if (t <= LEAVE) return { scale: 1, opacity: 1, depth: 1, reading: true, drop: 0 };
  // Passing, it grows and drops below the camera, and fades fast so the next one reads.
  const by = (t - LEAVE) / (GONE - LEAVE);
  const depth = 1 - 0.6 * by;
  return { scale: 1 / depth, opacity: (1 - by) ** 2.5, depth, reading: false, drop: by * by };
}

// What a stop does while cruise control holds it; each returns how to undo it.
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

// The streamlined Omnibar, on every screen: the field and Ignite, the rows only once the reader
// types, Radio in the header and the theme picker on the theme card.
function streamline(drive) {
  const omnibar = drive.querySelector(".omnibar");
  const input = omnibar.querySelector(".omnibar__input");
  document.body.classList.add("streamlined");
  input.addEventListener("input", () =>
    omnibar.toggleAttribute("data-typed", Boolean(input.value)),
  );
  const themes = omnibar.querySelector(".themes");
  const themeCard = drive.querySelector("#theme");
  if (themes && themeCard) themeCard.append(themes);
  const ignite = document.createElement("button");
  ignite.className = "omnibar__ignite";
  ignite.type = "button";
  ignite.innerHTML = '<span aria-hidden="true">&#9654;</span> Ignite';
  omnibar.querySelector(".omnibar__field").append(ignite);
  return ignite;
}

export function igniteTourPrototype({ drive, host }) {
  const radio = document.querySelector(".radio");
  const radioToggle = document.querySelector(".radio-toggle");
  if (!radio || !radioToggle) return;
  const ignite = streamline(drive);
  const still = matchMedia(STILL_MEDIA);
  if (still.matches) {
    ignite.hidden = true;
    return;
  }

  // The highway: every stop a layer over the road, and the page's height a track to scroll.
  document.documentElement.classList.add("highway");
  const layers = STOPS.map((stop) => drive.querySelector(stop.target));
  for (const layer of layers) layer.classList.add("hw-layer");
  const track = document.createElement("div");
  track.className = "hw-track";
  drive.prepend(track);
  let base = 0;
  let step = innerHeight;
  const measure = () => {
    step = innerHeight;
    track.style.height = `${(STOPS.length - 1) * step + innerHeight}px`;
    track.replaceChildren(
      ...STOPS.map((_, index) => {
        const mark = document.createElement("i");
        mark.style.top = `${index * step}px`;
        return mark;
      }),
    );
    base = track.getBoundingClientRect().top + scrollY;
  };
  const progress = () => (scrollY - base) / step;
  let reading = 0;
  const render = () => {
    const at = progress();
    layers.forEach((layer, index) => {
      const place = placement(at - index, index === layers.length - 1);
      if (!place) {
        layer.style.display = "none";
        return;
      }
      layer.style.display = "";
      layer.style.opacity = place.opacity.toFixed(3);
      layer.style.zIndex = String(100 - Math.round(place.depth * 5));
      layer.style.setProperty("--hw-scale", place.scale.toFixed(4));
      layer.style.setProperty("--hw-drop", `${(place.drop * 60).toFixed(2)}vh`);
      layer.toggleAttribute("data-reading", place.reading);
      if (place.reading) reading = index;
    });
  };
  let frame = 0;
  addEventListener(
    "scroll",
    () => {
      if (!frame) frame = requestAnimationFrame(() => ((frame = 0), render()));
    },
    { passive: true },
  );
  addEventListener("resize", () => {
    measure();
    render();
  });
  measure();
  render();

  // Scroll is the throttle: the road runs with the scroll's speed, harder than on the stacked page.
  let cruising = 0;
  const setNavigating = host.setNavigating.bind(host);
  host.setNavigating = (value) => setNavigating(Math.min(1, Math.max(value * 4, cruising)));

  // A glide to a stop, as cruise control or the Omnibar drives: eased, the road at speed, and the
  // snap let go until it lands.
  let glide = 0;
  const glideTo = (index, length = 2200) => {
    cancelAnimationFrame(glide);
    const from = scrollY;
    const to = base + index * step;
    const began = performance.now();
    document.documentElement.style.scrollSnapType = "none";
    const tick = (now) => {
      const t = Math.min(1, (now - began) / length);
      const eased = t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) ** 3 / 2;
      cruising = Math.sin(Math.PI * t);
      scrollTo({ top: from + (to - from) * eased, behavior: "instant" });
      if (t < 1) glide = requestAnimationFrame(tick);
      else {
        glide = 0;
        cruising = 0;
        document.documentElement.style.scrollSnapType = "";
      }
    };
    glide = requestAnimationFrame(tick);
  };

  const chip = document.createElement("div");
  chip.className = "tour-chip";
  chip.hidden = true;
  chip.innerHTML = `
    <div class="tour-chip__text">
      <span class="tour-chip__name"></span>
      <span class="tour-chip__line"><i></i></span>
    </div>
    <button class="tour-chip__resume" type="button">Resume cruise</button>
    <button class="tour-chip__stop" type="button">Stop <kbd>Esc</kbd></button>`;
  document.body.append(chip);
  const chipName = chip.querySelector(".tour-chip__name");
  const chipLine = chip.querySelector(".tour-chip__line i");
  const resumeButton = chip.querySelector(".tour-chip__resume");
  const bar = document.createElement("div");
  bar.className = "proto-bar";
  document.body.append(bar);

  // off, cruising, held (the reader has the wheel, the music plays on) or parked (at Install,
  // the song playing out).
  let state = "off";
  let current = -1;
  let undo = () => {};
  let timerMode = false;
  let timerStart = 0;
  const clock = () =>
    timerMode ? (performance.now() - timerStart) / 1000 : radio.currentTime || 0;
  const seek = (seconds) => {
    if (timerMode) timerStart = performance.now() - seconds * 1000;
    else radio.currentTime = seconds;
  };
  const clearStop = () => {
    undo();
    undo = () => {};
  };
  const goTo = (index) => {
    clearStop();
    current = index;
    glideTo(index);
    undo = STOPS[index].perform?.(layers[index]) || (() => {});
  };

  const show = () => {
    chip.hidden = state === "off";
    resumeButton.hidden = state !== "held";
    chipName.textContent =
      state === "held"
        ? "You have the wheel"
        : state === "parked"
          ? "End of the road"
          : STOPS[current]?.name;
    const time = clock();
    chipLine.style.width = `${Math.min(100, (time / STOPS[STOPS.length - 1].cue) * 100)}%`;
    const clockText = `${Math.floor(time / 60)}:${String(Math.floor(time % 60)).padStart(2, "0")}.${Math.floor((time % 1) * 10)}`;
    bar.textContent = `PROTOTYPE · ${timerMode ? "timer" : "song"} ${clockText} · ${state} · reading ${reading + 1}/${STOPS.length} ${STOPS[reading].name}`;
  };

  const tick = () => {
    if (state === "cruising") {
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
    radio.loop = true;
    show();
  };

  const start = () => {
    clearStop();
    current = -1;
    state = "cruising";
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

  // The reader takes the wheel: cruise lets go where it is and the music plays on, on its loop.
  const hold = () => {
    if (state !== "cruising") return;
    state = "held";
    clearStop();
    cancelAnimationFrame(glide);
    cruising = 0;
    document.documentElement.style.scrollSnapType = "";
    radio.loop = true;
    show();
  };

  // Resume from the stop being read, the song sought to its cue.
  const resume = () => {
    const nearest = Math.max(0, Math.min(STOPS.length - 1, Math.round(progress())));
    radio.loop = false;
    seek(STOPS[nearest].cue + 0.01);
    current = -1;
    state = "cruising";
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
  radio.addEventListener("pause", () => {
    if (state !== "off" && !radio.ended) end();
  });
  radio.addEventListener("ended", () => state !== "off" && end());

  // The Omnibar drives too: Return on a row glides to its feature, on an empty field it ignites,
  // and J and K glide to the next and the previous stop.
  const input = drive.querySelector(".omnibar__input");
  const stopOfRow = (row) =>
    STOPS.findIndex((stop) => stop.target === `#${row.querySelector("a").dataset.card}`);
  addEventListener(
    "keydown",
    (event) => {
      if (!event.isTrusted) return;
      const ours = event.target.closest?.(".tour-chip");
      if (event.key === "Escape" && state !== "off") {
        event.stopImmediatePropagation();
        return stopAll();
      }
      if (event.target === input && event.key === "Enter") {
        event.preventDefault();
        event.stopImmediatePropagation();
        if (!input.value.trim()) return start();
        const row = [...drive.querySelectorAll('.omnibar [role="option"]')].find(
          (option) => !option.hidden && option.getAttribute("aria-selected") === "true",
        );
        hold();
        if (row) glideTo(stopOfRow(row), 2600);
        return;
      }
      if (ours || event.key.toLowerCase() === "m") return;
      hold();
      const key = event.key.toLowerCase();
      if ((key === "j" || key === "k") && !event.target.closest?.("input")) {
        event.stopImmediatePropagation();
        const next = Math.max(0, Math.min(STOPS.length - 1, reading + (key === "j" ? 1 : -1)));
        glideTo(next, 1600);
      }
    },
    { capture: true },
  );
  addEventListener(
    "click",
    (event) => {
      const link = event.target.closest?.(".omnibar__rows a");
      if (!link || !event.isTrusted) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      hold();
      glideTo(stopOfRow(link.closest('[role="option"]')), 2600);
    },
    { capture: true },
  );
  for (const type of ["wheel", "touchstart", "pointerdown"]) {
    addEventListener(
      type,
      (event) => {
        if (!event.isTrusted) return;
        if (event.target.closest?.(".omnibar__ignite, .tour-chip, .omnibar__rows")) return;
        hold();
      },
      { capture: true, passive: true },
    );
  }
  show();
}
