// PROTOTYPE, throwaway (branch feat/ignite-tour-prototype): driving through the site. On a wide
// screen that allows motion, the page is a road: scroll is the throttle, and each stop (the Start
// page, the film, every feature, the install) rises from the horizon, holds while it is read and
// passes the camera. The Radio is the ignition: turned on, from the header, M or Return on an
// empty Omnibar, it is cruise control, the same drive steered by Night road's cues. Anything the reader does takes the wheel back and leaves the music on.
// ?clock=timer drives on a timer, as it does when the radio cannot play. On a phone or under
// reduced motion the page stays stacked, with the streamlined Omnibar and no drive.

import { STILL_MEDIA } from "./scene.js";

// Night road runs at about 107.5 beats a minute, so a bar is 2.23 seconds; the beat comes in after
// eight bars and the sections turn every eight. Each card holds four bars, and the install lands on
// the section at bar 40. Tune by ear: the prototype bar shows the song's time.
//
// `clip` is the card's beat of the introduction film, in seconds of the cut, from its captions
// (build/film/captions.vtt): the card plays it in place of its capture while it is read.
const BAR = 240 / 107.5;
const STOPS = [
  { name: "Start", bars: 0, target: ".start", perform: typeQuery },
  { name: "One field for everything", bars: 8, target: "#omnibar", clip: [0, 9.4] },
  { name: "Work and personal, kept apart", bars: 12, target: "#spaces", clip: [13.2, 19.7] },
  { name: "Keyboard first", bars: 16, target: "#keyboard", clip: [9.4, 13.2], perform: lightKeys },
  { name: "Follows your Omarchy theme", bars: 20, target: "#theme", clip: [34.1, 50.6] },
  { name: "Private by default", bars: 24, target: "#private" },
  { name: "Blocking, built in", bars: 28, target: "#blocking", clip: [23.9, 34.1] },
  { name: "Split view and docked DevTools", bars: 32, target: "#pros", clip: [19.7, 23.9] },
  { name: "Agents drive their own Spaces", bars: 36, target: "#agents", clip: [50.6, 61] },
  { name: "Get on the road", bars: 40, target: "#install" },
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

// Keyboard first: its keys light one by one while the film shows the hints.
function lightKeys(card) {
  const timers = [];
  const keys = [...card.querySelectorAll("kbd")];
  keys.forEach((key, at) => later(timers, 0.6 + at * 0.4, () => key.classList.add("tour-key")));
  return () => {
    timers.forEach(clearTimeout);
    keys.forEach((key) => key.classList.remove("tour-key"));
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
}

// The Omnibar's rim and bloom on every card's border too: the road's light, cast on the Omnibar,
// copied to each card on screen, so the cards' rims catch the sun and lift with the beat.
function lightTheCards(drive, host) {
  const cards = [...drive.querySelectorAll(".card, .install, .igniter--start")];
  for (const card of cards) {
    const glow = document.createElement("span");
    glow.className = "omnibar__glow";
    glow.setAttribute("aria-hidden", "true");
    card.prepend(glow);
    card.classList.add("lit-card");
  }
  const light = host.light;
  if (!light) return;
  const cast = light.cast.bind(light);
  const written = new Map(cards.map((card) => [card, {}]));
  light.cast = (properties) => {
    cast(properties);
    for (const card of cards) {
      if (card.style.display === "none") continue;
      const was = written.get(card);
      for (const [name, value] of Object.entries(properties)) {
        if (was[name] === value) continue;
        card.style.setProperty(name, value);
        was[name] = value;
      }
    }
  };
}

// Ignite, three ways, on ?ignite= and the prototype bar's arrows: "start", a round engine-start
// button on the road under the Omnibar; "cta", a second call to action beside the install; and
// "road", a marker set into the asphalt where the centre line runs out.
const IGNITERS = { a: "start", b: "cta", c: "road" };
const igniterKey = Object.keys(IGNITERS).includes(
  new URLSearchParams(location.search).get("ignite"),
)
  ? new URLSearchParams(location.search).get("ignite")
  : "a";

function switchIgniter(by) {
  const keys = Object.keys(IGNITERS);
  const params = new URLSearchParams(location.search);
  params.set("ignite", keys[(keys.indexOf(igniterKey) + by + keys.length) % keys.length]);
  location.search = params.toString();
}

function buildIgniter(drive) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = `igniter igniter--${IGNITERS[igniterKey]}`;
  if (igniterKey === "a") {
    button.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3v8M7.05 6.05a7 7 0 1 0 9.9 0" /></svg>
      <span>Engine<br />start</span>`;
    button.setAttribute("aria-label", "Engine start: drive through Omaweb with the radio on");
    drive.querySelector(".start__deck").append(button);
  } else if (igniterKey === "b") {
    button.className += " btn";
    button.innerHTML = '<span aria-hidden="true">&#9654;</span> Take the drive';
    drive.querySelector(".start__below").prepend(button);
  } else {
    button.innerHTML =
      '<span class="igniter__mark"><span aria-hidden="true">&#9654;</span> Ignite</span>';
    button.setAttribute("aria-label", "Ignite: drive through Omaweb with the radio on");
    drive.querySelector(".start").append(button);
  }
  return button;
}

// The film, cut into the cards: one video, carried into the capture of the card being read, which
// plays its beat from the start and holds on its last frame. A card without a beat shows its
// capture. Leaving a card keeps the footage on it while it passes.
function rollFootage(layers) {
  const video = document.createElement("video");
  video.className = "card__footage";
  video.muted = true;
  video.playsInline = true;
  video.preload = "auto";
  video.setAttribute("aria-hidden", "true");
  for (const [type, file] of [
    ["video/webm", "omaweb.webm"],
    ["video/mp4", "omaweb.mp4"],
  ]) {
    const source = document.createElement("source");
    source.type = type;
    source.src = `assets/film/${file}`;
    video.append(source);
  }
  let shown = -1;
  let end = 0;
  video.addEventListener("timeupdate", () => {
    if (video.currentTime >= end) video.pause();
  });
  return (index) => {
    if (index === shown) return;
    shown = index;
    const clip = STOPS[index].clip;
    if (!clip) return;
    layers[index].querySelector(".card__shot").append(video);
    end = clip[1];
    video.currentTime = clip[0];
    video.play().catch(() => {});
  };
}

export function igniteTourPrototype({ drive, host }) {
  const radio = document.querySelector(".radio");
  const radioToggle = document.querySelector(".radio-toggle");
  if (!radio || !radioToggle) return;
  streamline(drive);
  const igniter = buildIgniter(drive);
  lightTheCards(drive, host);
  if (matchMedia(STILL_MEDIA).matches) {
    igniter.hidden = true;
    return;
  }
  const input = drive.querySelector(".omnibar__input");
  input.placeholder = "Where to?   \u23ce to ignite";

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
  const footage = rollFootage(layers);
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
    footage(reading);
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
  bar.innerHTML = `
    <button type="button" data-by="-1" aria-label="Previous Ignite">&larr;</button>
    <span class="proto-bar__variant"></span>
    <button type="button" data-by="1" aria-label="Next Ignite">&rarr;</button>
    <span class="proto-bar__state"></span>`;
  document.body.append(bar);
  bar.querySelector(".proto-bar__variant").textContent = `ignite=${IGNITERS[igniterKey]}`;
  const barState = bar.querySelector(".proto-bar__state");
  for (const button of bar.querySelectorAll("[data-by]")) {
    button.addEventListener("click", () => switchIgniter(Number(button.dataset.by)));
  }
  igniter.addEventListener("click", () => ignite());

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
    barState.textContent = `PROTOTYPE · ${timerMode ? "timer" : "song"} ${clockText} · ${state} · reading ${reading + 1}/${STOPS.length} ${STOPS[reading].name}`;
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

  // The radio turned on is the ignition: the song from its start, and cruise control with it.
  const start = (timer = forceTimer) => {
    clearStop();
    current = -1;
    state = "cruising";
    timerMode = timer;
    timerStart = performance.now();
    radio.loop = false;
    if (!timer) radio.currentTime = 0;
    tick();
  };
  radio.addEventListener("play", () => {
    if (state === "off") start();
  });
  // Return on an empty Omnibar presses the Radio, from inside the press so its analyser starts and
  // the sun beats. A radio that does not play, as when autoplay is refused, leaves the drive to the
  // timer.
  const ignite = () => {
    if (state !== "off") return;
    igniter.toggleAttribute("data-pressed", true);
    if (radio.paused) radioToggle.click();
    setTimeout(() => state === "off" && radio.paused && start(true), 1500);
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

  resumeButton.addEventListener("click", resume);
  chip.querySelector(".tour-chip__stop").addEventListener("click", stopAll);
  radio.addEventListener("pause", () => {
    if (state !== "off" && !radio.ended) end();
  });
  radio.addEventListener("ended", () => state !== "off" && end());

  // The Omnibar drives too: Return on a row glides to its feature, on an empty field it ignites,
  // and J and K glide to the next and the previous stop.
  const stopOfRow = (row) =>
    STOPS.findIndex((stop) => stop.target === `#${row.querySelector("a").dataset.card}`);
  addEventListener(
    "keydown",
    (event) => {
      if (!event.isTrusted) return;
      const ours = event.target.closest?.(".tour-chip");
      const arrow = event.key === "ArrowLeft" ? -1 : event.key === "ArrowRight" ? 1 : 0;
      if (arrow && !event.target.closest?.("input")) return switchIgniter(arrow);
      if (event.key === "Escape" && state !== "off") {
        event.stopImmediatePropagation();
        return stopAll();
      }
      if (event.target === input && event.key === "Enter") {
        event.preventDefault();
        event.stopImmediatePropagation();
        if (!input.value.trim()) return ignite();
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
        if (
          event.target.closest?.(".radio-toggle, .tour-chip, .omnibar__rows, .igniter, .proto-bar")
        )
          return;
        hold();
      },
      { capture: true, passive: true },
    );
  }
  show();
}
