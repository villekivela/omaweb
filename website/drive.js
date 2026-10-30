// The landing page's behaviour: the road behind it, the Omnibar on the Start page, the
// introduction and the theme, and on a wide screen that allows motion the drive down the road;
// and on a release page, the road as a still header. Optional, like all of the page's script:
// without it the road is a gradient, the page is stacked, the Omnibar's rows are links to their
// cards, and the introduction is an ordinary video.

import { createCrtRoad } from "./crt-road.js";
import { filmKey, filmState, seekAt } from "./film.js";
import {
  Cruise,
  REVEAL_EASING,
  REVEAL_MS,
  cues,
  keyAnswer,
  nearestStop,
  placement,
  revealClip,
  wornAt,
} from "./highway.js";
import { STILL_MEDIA, SceneHost } from "./scene.js";

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
    : { setNavigating() {}, lightUp() {} };

  // Past the Start page the road sinks under a scrim of the theme's ground so the cards read
  // against it, and the scroll's speed is how hard the reader is navigating: the road speeds up
  // with it and eases back when the scroll stops. On the drive the scroll is the throttle, so the
  // road answers it harder, and a glide between stops runs it at speed (`cruising`).
  const start = drive.querySelector(".start");
  const scrim = drive.querySelector(".drive__scrim");
  let lastY = scrollY;
  let lastAt = performance.now();
  let pace = 0;
  let easing = 0;
  let throttle = 1;
  let cruising = 0;
  const navigate = () => host.setNavigating(Math.min(1, Math.max(pace * throttle, cruising)));
  const settle = () => {
    pace *= 0.9;
    navigate();
    easing = pace > 0.01 ? requestAnimationFrame(settle) : 0;
  };
  const scrolled = () => {
    const now = performance.now();
    pace = Math.max(pace, Math.min(1, Math.abs(scrollY - lastY) / Math.max(16, now - lastAt) / 3));
    lastY = scrollY;
    lastAt = now;
    navigate();
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
    // Read from the press, since older WebKit's click carries no pointer type.
    let pressedBy = "mouse";
    film.addEventListener("pointerdown", (event) => {
      pressedBy = event.pointerType;
    });
    film.addEventListener("click", () => {
      // A first tap on a touch screen only shows the controls; a click plays or pauses.
      if (pressedBy === "touch" && !frame.hasAttribute("data-awake")) wake();
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

  // The Omnibar: type to filter the features, arrows to choose a row, Return to go to its card. At
  // rest it is the field alone, its rows showing once the reader types. On the drive, Return on
  // the empty field is the ignition.
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
    if (highway) highway.goTo(card);
    else card.scrollIntoView({ behavior: smooth ? "smooth" : "auto", block: "center" });
  };
  input.addEventListener("input", () => {
    omnibar.toggleAttribute("data-quiet", !input.value);
    const query = input.value.trim().toLowerCase();
    for (const row of rows)
      row.hidden = Boolean(query) && !row.textContent.toLowerCase().includes(query);
    choose(0);
  });
  input.addEventListener("keydown", (event) => {
    if (event.key === "ArrowDown" || event.key === "ArrowUp") {
      event.preventDefault();
      choose(chosen + (event.key === "ArrowDown" ? 1 : -1));
    } else if (event.key === "Enter" && highway && !input.value.trim()) {
      event.preventDefault();
      highway.ignite();
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
  omnibar.toggleAttribute("data-quiet", true);

  // The theme: the whole page, its road and its captures take the chosen Omarchy theme. A capture
  // with no data-shot shows a theme of its own, as the theme card's do.
  const captures = [...drive.querySelectorAll("img[data-shot]")];
  let theme = document.documentElement.dataset.theme;
  const picker = drive.querySelector(".themes");
  const swatches = [...picker.querySelectorAll("[data-theme-choice]")];
  // A change of theme opens with the desktop's diagonal reveal, a View Transition whose new view
  // grows from a slanted line through the viewport's middle: the film's, and the reader's own. With
  // reduced motion, or no View Transitions, `update` runs at once. Resolves when it is over.
  const still = matchMedia("(prefers-reduced-motion: reduce)");
  const withReveal = async (update) => {
    if (still.matches || !document.startViewTransition) return update();
    const transition = document.startViewTransition(update);
    transition.ready
      .then(() => {
        const { innerWidth: width, innerHeight: height } = window;
        document.documentElement.animate(
          { clipPath: [revealClip(0, width, height), revealClip(1, width, height)] },
          {
            duration: REVEAL_MS,
            easing: REVEAL_EASING,
            fill: "forwards",
            pseudoElement: "::view-transition-new(root)",
          },
        );
      })
      .catch(() => {});
    await transition.finished.catch(() => {});
  };
  // The pick lasts the visit, so the release pages open in it, and a new visit opens in the
  // markup's theme again; theme.js applies it.
  const paint = (next, remember = true, reveal = false) => {
    theme = next;
    if (remember) {
      try {
        sessionStorage.setItem("omaweb-theme", theme);
      } catch {
        // Storage can be blocked; the pick then lasts as long as the page.
      }
    }
    const dress = () => {
      document.documentElement.dataset.theme = theme;
      for (const swatch of swatches) {
        const on = swatch.dataset.themeChoice === theme;
        swatch.setAttribute("aria-pressed", String(on));
        if (on) picker.querySelector(".themes__name").textContent = swatch.dataset.name;
      }
      for (const image of captures) {
        image.src = `assets/shots/${theme}/${image.dataset.shot}.webp`;
      }
      // The reveal waits for the new captures, so it does not open on empty frames.
      return Promise.all(captures.map((image) => image.decode().catch(() => {})));
    };
    return reveal ? withReveal(dress) : dress();
  };
  for (const swatch of swatches) {
    swatch.addEventListener("click", () => paint(swatch.dataset.themeChoice, true, true));
  }
  picker.hidden = false;
  paint(theme, false);

  // J and K: the next and the previous card, as they move through a Space's tabs. The card in
  // hand is the one last moved to while it is still on screen, else the one nearest the middle.
  const cards = [...drive.querySelectorAll(".card")];
  let current = -1;
  const step = (by) => {
    if (highway) return highway.step(by);
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
      const next = swatches[(at + by + swatches.length) % swatches.length];
      paint(next.dataset.themeChoice, true, true);
    }
  });

  // The drive, on a wide screen that allows motion: the page is the road (highway.js). Its stops
  // are the Start page, each card and the install; the page's height becomes a track to scroll,
  // and each stop a layer over the road, placed by how far along it the scroll is. A phone or a
  // reader who asked for less motion keeps the stacked page, decided once as the page opens.
  const highway = matchMedia(STILL_MEDIA).matches ? null : driveTheRoad();

  function driveTheRoad() {
    const root = document.documentElement;
    const stops = [start, ...cards, drive.querySelector(".install")];
    const times = cues(stops.length);
    root.classList.add("highway");
    for (const stop of stops) stop.classList.add("highway__stop");
    throttle = 4;

    // One window's height of scroll to each stop, with a snap where it is read.
    const track = document.createElement("div");
    track.className = "highway__track";
    track.setAttribute("aria-hidden", "true");
    drive.prepend(track);
    let base = 0;
    let length = innerHeight;
    const measure = () => {
      length = innerHeight;
      track.style.height = `${(stops.length - 1) * length + innerHeight}px`;
      track.replaceChildren(
        ...stops.map((_, index) => {
          const mark = document.createElement("i");
          mark.style.top = `${index * length}px`;
          return mark;
        }),
      );
      base = track.getBoundingClientRect().top + scrollY;
    };
    const progress = () => (scrollY - base) / length;

    // The cards catch the sun as the Omnibar does, each with a bloom of its own, and the install's
    // Omnibar as the Start page's does, its bloom its own.
    for (const stop of stops.slice(1)) {
      if (!stop.querySelector(".omnibar__glow")) {
        const glow = document.createElement("span");
        glow.className = "stop__glow";
        glow.setAttribute("aria-hidden", "true");
        stop.prepend(glow);
      }
      host.lightUp(stop);
    }

    // The film, a clip to each card that has a beat of it (film/README.md): the clip's first frame
    // from the horizon on, played from its start when the card is read, held on its last frame. A
    // clip that names the themes it was recorded in has the page wear each from its frame on, and
    // the page's own theme back when its card is no longer read. A theme the page has no rule for
    // is not worn. Without `clips.json` the cards keep their captures.
    const clips = new Map();
    fetch(new URL("assets/film/clips.json", document.baseURI))
      .then((response) => (response.ok ? response.json() : {}))
      .then((listed) => {
        for (const [name, clip] of Object.entries(listed)) {
          const card = cards.find((candidate) => candidate.id === name);
          if (!card) continue;
          const video = document.createElement("video");
          video.className = "card__clip";
          video.muted = true;
          video.playsInline = true;
          video.preload = "auto";
          video.poster = `assets/film/clip-${name}.webp`;
          video.setAttribute("aria-hidden", "true");
          for (const [type, kind] of [
            ["video/webm", "webm"],
            ["video/mp4", "mp4"],
          ]) {
            const source = document.createElement("source");
            source.type = type;
            source.src = `assets/film/clip-${name}.${kind}`;
            video.append(source);
          }
          card.querySelector(".card__shot").append(video);
          clips.set(card, {
            video,
            themes: (clip.themes || []).filter(([, worn]) => styled(worn)),
          });
        }
        rolled = -1;
        render();
      })
      .catch(() => {});
    // Whether the page has a rule for a theme: its ground on a probe that inherits none, so that
    // the theme the page already wears, which the probe would inherit, still counts as styled.
    const bare = document.createElement("i");
    bare.hidden = true;
    bare.style.setProperty("--bg", "initial");
    const probe = document.createElement("i");
    bare.append(probe);
    drive.append(bare);
    const styled = (worn) => {
      probe.dataset.theme = worn;
      return getComputedStyle(probe).getPropertyValue("--bg").trim() !== "";
    };
    const named = picker.querySelector(".themes__name");
    const dress = (worn) => {
      root.dataset.theme = worn;
      const swatch = swatches.find((choice) => choice.dataset.themeChoice === worn);
      named.textContent =
        swatch?.dataset.name ??
        worn.replace(/(^|-)([a-z])/g, (_, gap, letter) => (gap ? " " : "") + letter.toUpperCase());
    };
    // The theme a reveal is on its way to: the page takes it a frame after the transition starts,
    // and until then `follow` would see the old theme and start the reveal again.
    let revealing = "";
    const wear = (worn, reveal = false) => {
      if (revealing === worn) return;
      revealing = "";
      if (!reveal) return dress(worn);
      revealing = worn;
      withReveal(() => dress(worn)).finally(() => {
        if (revealing === worn) revealing = "";
      });
    };
    let following = 0;
    let rolled = -1;
    const roll = (index) => {
      if (index === rolled) return;
      rolled = index;
      cancelAnimationFrame(following);
      if (root.dataset.theme !== theme) wear(theme);
      const clip = clips.get(stops[index]);
      if (!clip) return;
      clip.video.currentTime = 0;
      clip.video.play().catch(() => {});
      if (!clip.themes.length) return;
      // A theme the reader picks while the clip plays is theirs, and the clip stops dressing the page.
      const picked = theme;
      const follow = () => {
        if (theme !== picked) return;
        const worn = wornAt(clip.video.currentTime, clip.themes);
        if (worn && root.dataset.theme !== worn) wear(worn, true);
        following = clip.video.ended ? 0 : requestAnimationFrame(follow);
      };
      following = requestAnimationFrame(follow);
    };

    let reading = 0;
    const render = () => {
      const at = progress();
      stops.forEach((stop, index) => {
        const place = placement(at - index, index === stops.length - 1);
        if (!place) {
          stop.style.display = "none";
          return;
        }
        stop.style.display = "";
        stop.style.opacity = place.opacity.toFixed(3);
        stop.style.zIndex = String(100 - Math.round(place.depth * 5));
        stop.style.setProperty("--stop-scale", place.scale.toFixed(4));
        stop.style.setProperty("--stop-drop", `${(place.drop * 60).toFixed(2)}vh`);
        stop.toggleAttribute("data-reading", place.reading);
        if (place.reading) reading = index;
      });
      roll(reading);
    };
    let frame = 0;
    addEventListener(
      "scroll",
      () => {
        if (!frame) frame = requestAnimationFrame(() => ((frame = 0), render()));
        if (resting !== null && !gliding && Math.abs(scrollY - resting) > 8) takeWheel();
      },
      { passive: true },
    );
    addEventListener("resize", () => {
      resting = null;
      measure();
      render();
    });
    measure();
    render();

    // A glide to a stop, as cruise control, the Omnibar or J and K drive: eased, the road at
    // speed, and the snap let go until it lands.
    let gliding = 0;
    // Where a glide left the scroll, while cruise control rests there: a scroll away from it that
    // no glide made, as the scrollbar dragged, is the reader's hand (`takeWheel`), as is one in
    // the middle of a glide.
    let resting = null;
    let takeWheel = () => letGo();
    const letGo = () => {
      cancelAnimationFrame(gliding);
      gliding = 0;
      cruising = 0;
      resting = null;
      root.style.scrollSnapType = "";
    };
    const glide = (index, duration = 2200) => {
      cancelAnimationFrame(gliding);
      const from = scrollY;
      const to = base + index * length;
      const began = performance.now();
      root.style.scrollSnapType = "none";
      let placed = from;
      const move = (now) => {
        // The scrollbar dragged, or any scroll the glide did not make, is the reader's hand.
        if (Math.abs(scrollY - placed) > 2) return takeWheel();
        const t = Math.min(1, (now - began) / duration);
        const eased = t < 0.5 ? 4 * t ** 3 : 1 - (-2 * t + 2) ** 3 / 2;
        cruising = Math.sin(Math.PI * t);
        placed = Math.round(from + (to - from) * eased);
        scrollTo({ top: placed, behavior: "instant" });
        if (t < 1) gliding = requestAnimationFrame(move);
        else {
          letGo();
          resting = placed;
        }
      };
      gliding = requestAnimationFrame(move);
    };

    // Cruise control, on the night radio: its button in the header, M, Ignite at the Omnibar's end
    // and Return on the empty Omnibar all turn it on, and the radio turned on drives the road on
    // the song's cues from its start. A radio that does not play, as when the browser refuses it,
    // leaves the drive to a timer on the same cues. Anything the reader does takes the wheel: the
    // drive lets go and the music plays on, on its loop again. Stop, Esc or the radio turned off
    // ends both.
    const radio = document.querySelector(".radio");
    const radioButton = document.querySelector(".radio-toggle");
    const ignition = omnibar.querySelector(".omnibar__ignite");
    if (!radio || !radioButton) return { goTo() {}, step() {}, ignite() {} };
    const cruise = new Cruise(times);
    let timed = false;
    let timerStart = 0;
    const clock = () => (timed ? (performance.now() - timerStart) / 1000 : radio.currentTime);

    const chip = document.createElement("div");
    chip.className = "cruise";
    chip.hidden = true;
    chip.innerHTML = `<span class="cruise__name"></span
      ><button class="cruise__resume" type="button">Resume cruise</button
      ><button class="cruise__stop" type="button">Stop <span class="cruise__key">Esc</span></button
      ><span class="cruise__song" aria-hidden="true"><i></i></span>`;
    document.body.append(chip);
    const where = chip.querySelector(".cruise__name");
    const song = chip.querySelector(".cruise__song");
    const resumeButton = chip.querySelector(".cruise__resume");
    const nameOf = (index) =>
      index === 0 ? "Start" : stops[index].querySelector("h2").textContent.replace(/\.$/, "");
    const show = () => {
      chip.hidden = cruise.state === "off";
      resumeButton.hidden = cruise.state !== "held";
      where.textContent =
        cruise.state === "held"
          ? "You have the wheel"
          : cruise.state === "parked"
            ? "End of the road"
            : nameOf(Math.max(0, cruise.current));
      song.style.setProperty("--song", Math.min(1, clock() / times[times.length - 1]).toFixed(3));
    };

    // What a stop does as cruise control reaches it: the Omnibar types its way to a feature, and
    // Keyboard first lights its keys. Each gives back how to undo it.
    const later = (timers, seconds, action) => timers.push(setTimeout(action, seconds * 1000));
    const performs = new Map([
      [
        start,
        () => {
          const timers = [];
          const type = (value) => {
            input.value = value;
            input.dispatchEvent(new Event("input"));
          };
          const query = "keyboard";
          [...query].forEach((_, at) =>
            later(timers, 2 + at * 0.18, () => type(query.slice(0, at + 1))),
          );
          later(timers, 2 + (times[1] - 2) * 0.6, () => type(""));
          return () => {
            timers.forEach(clearTimeout);
            type("");
          };
        },
      ],
      [
        drive.querySelector("#keyboard"),
        (card) => {
          const timers = [];
          const keys = [...card.querySelectorAll("kbd")];
          keys.forEach((key, at) =>
            later(timers, 0.6 + at * 0.4, () => key.toggleAttribute("data-lit", true)),
          );
          return () => {
            timers.forEach(clearTimeout);
            for (const key of keys) key.removeAttribute("data-lit");
          };
        },
      ],
    ]);
    let undo = () => {};
    const leave = () => {
      undo();
      undo = () => {};
    };

    const tick = () => {
      const index = cruise.tick(clock());
      if (index !== null) {
        leave();
        glide(index);
        undo = performs.get(stops[index])?.(stops[index]) ?? (() => {});
        if (cruise.state === "parked" && timed)
          setTimeout(() => cruise.state === "parked" && end(), 4000);
      }
      show();
    };
    setInterval(() => cruise.state !== "off" && tick(), 100);

    const begin = (timer) => {
      leave();
      timed = timer;
      timerStart = performance.now();
      radio.loop = false;
      if (!timer) radio.currentTime = 0;
      cruise.start();
      tick();
    };
    const end = () => {
      leave();
      cruise.end();
      radio.loop = true;
      show();
    };
    // A glide in flight lets go whatever cruise control's state, as on the way to the install,
    // where it has already parked.
    const hold = () => {
      if (gliding) letGo();
      if (!cruise.hold()) return;
      leave();
      radio.loop = true;
      show();
    };
    const resume = () => {
      const cue = cruise.resume(nearestStop(progress(), stops.length)) + 0.01;
      radio.loop = false;
      if (timed) timerStart = performance.now() - cue * 1000;
      else {
        radio.currentTime = cue;
        if (radio.paused) radio.play().catch(() => {});
      }
      tick();
    };
    const stopAll = () => {
      end();
      if (!radio.paused) radio.pause();
    };
    // The radio's own button, pressed from inside the reader's press, so the song's analyser starts
    // and the sun beats with it.
    const ignite = () => {
      if (cruise.state !== "off") return;
      if (radio.paused) radioButton.click();
      setTimeout(() => cruise.state === "off" && radio.paused && begin(true), 1500);
    };

    takeWheel = hold;
    // The radio coming on while a timer drives, as when the reader turns it on after the browser
    // refused it, takes the drive over at the timer's place in the song.
    radio.addEventListener("play", () => {
      if (cruise.state === "off") begin(false);
      else if (timed) {
        radio.currentTime = clock();
        timed = false;
      }
    });
    radio.addEventListener("pause", () => cruise.state !== "off" && !radio.ended && end());
    radio.addEventListener("ended", () => cruise.state !== "off" && end());
    ignition.hidden = false;
    ignition.addEventListener("click", ignite);
    resumeButton.addEventListener("click", resume);
    chip.querySelector(".cruise__stop").addEventListener("click", stopAll);

    // The reader's own hand, before the page's keys act on it. The ignition, the chip, the radio
    // and the Omnibar's rows are cruise control's own controls or drive to a stop themselves.
    const own = (target) =>
      target instanceof Element &&
      target.closest(".omnibar__ignite, .cruise, .radio-toggle, .omnibar__rows");
    for (const type of ["wheel", "touchstart", "pointerdown"]) {
      addEventListener(type, (event) => event.isTrusted && !own(event.target) && hold(), {
        capture: true,
        passive: true,
      });
    }
    addEventListener(
      "keydown",
      (event) => {
        if (!event.isTrusted) return;
        const answer = keyAnswer(event.key, own(event.target));
        if (answer === "stop" && cruise.state !== "off") {
          event.stopImmediatePropagation();
          stopAll();
        } else if (answer === "hold") {
          hold();
        }
      },
      { capture: true },
    );

    return {
      goTo(card) {
        hold();
        glide(stops.indexOf(card), 2600);
      },
      step(by) {
        glide(Math.max(0, Math.min(stops.length - 1, reading + by)), 1600);
      },
      ignite,
    };
  }
}

// A release page: the road as a thin header, one still frame without the glass, in the reader's
// palette.
const header = document.querySelector(".log__road canvas");
if (header && crtRoad) new SceneHost(header, crtRoad, { held: true });
