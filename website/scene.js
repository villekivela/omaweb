// The Scene host: what stands behind the landing page's Start page. A Scene is a drawing and
// nothing else. The host owns the canvas and the clock, decides when a Scene may draw, and hands
// it everything it may know; README.md has the contract. The decisions are plain functions so
// they can be tested without a browser.

// A colour as hex, as the landing page's themes give it, or as rgb(), as Omaweb gives a page the
// reader's palette.
function rgb(value) {
  const channels = value.match(/^\s*rgba?\(\s*([\d.]+)[\s,]+([\d.]+)[\s,]+([\d.]+)/);
  if (channels) return channels.slice(1, 4).map(Number);
  let hex = value.trim().replace(/^#/, "");
  if (hex.length === 3) hex = [...hex].map((digit) => digit + digit).join("");
  return [0, 2, 4].map((at) => parseInt(hex.slice(at, at + 2), 16));
}

function lightness([red, green, blue]) {
  return (Math.max(red, green, blue) + Math.min(red, green, blue)) / 2 / 255;
}

// Whether the Scene holds still: the reader asked for less motion, the page is on a phone, where
// motion would cost battery for a page read in passing, or the page holds it still, as a release
// page's header does.
export function holdsStill(environment) {
  return Boolean(environment.reducedMotion || environment.phone || environment.held);
}

// What a Scene receives for one frame, from the theme's roles as CSS colours and the canvas's
// size in CSS pixels.
export function sceneInput(scene, environment) {
  const pitch = scene.pitch || 1;
  const still = holdsStill(environment);
  // A role the page does not name, as `--keyword` on an element that wears no theme, is left out.
  const palette = Object.fromEntries(
    Object.entries(environment.palette)
      .filter(([, colour]) => colour.trim())
      .map(([role, colour]) => [role, rgb(colour)]),
  );
  return {
    width: Math.max(1, Math.ceil(environment.width / pitch)),
    height: Math.max(1, Math.ceil(environment.height / pitch)),
    pitch,
    time: environment.time || 0,
    navigating: still ? 0 : Math.max(0, Math.min(1, environment.navigating || 0)),
    // The road stands still, as the hero's does until the journey starts; the sky carries on.
    parked: Boolean(environment.parked),
    // How high the sun stands, 0 to 1 of its height: the intro raises it.
    rise: still ? 1 : Math.max(0, Math.min(1, environment.rise ?? 1)),
    // How far the stars streak into speed lines, 0 to 1, as the drive sets off.
    streak: still ? 0 : Math.max(0, Math.min(1, environment.streak || 0)),
    // The hush before the drop, 0 to 1: the sky stills with the road.
    hush: still ? 0 : Math.max(0, Math.min(1, environment.hush || 0)),
    beat: still ? 0 : Math.max(0, Math.min(1, environment.beat || 0)),
    reducedMotion: still,
    palette,
    dark: lightness(palette.ground) <= 0.6,
    options: Object.fromEntries(
      Object.entries(scene.options || {}).map(([name, values]) => {
        const choice = environment.chosen?.[name];
        return [name, values.includes(choice) ? choice : values[0]];
      }),
    ),
  };
}

// The light a Scene casts on the page, as CSS custom properties: each colour it names as rgb(),
// each amount to two places, so a frame that changes nothing a reader could see writes nothing,
// and a CSS value as it is. A Scene that casts no light has none.
export function lightProperties(scene, input) {
  return Object.fromEntries(
    Object.entries(scene.light?.(input) || {}).map(([name, value]) => [
      `--scene-${name}`,
      typeof value === "string"
        ? value
        : Array.isArray(value)
          ? `rgb(${value.map(Math.round).join(" ")})`
          : value.toFixed(2),
    ]),
  );
}

// The Scene's light on one element of the page, through its style. It writes only what changed,
// and nothing while the element is off screen, so a frame costs the page no style work it cannot
// show; back on screen, the element catches up on the latest light. Given an element, it watches
// whether it is on screen; given only a style, as the tests give it, `setOnScreen` says.
export class SceneLight {
  constructor(element) {
    this.style = element.style;
    this.onScreen = false;
    this.latest = {};
    this.written = {};
    if (element.nodeType) {
      new IntersectionObserver((entries) => {
        this.setOnScreen(entries[entries.length - 1].isIntersecting);
      }).observe(element);
    }
  }

  cast(properties) {
    this.latest = properties;
    if (!this.onScreen) return;
    for (const [name, value] of Object.entries(properties)) {
      if (this.written[name] === value) continue;
      this.style.setProperty(name, value);
      this.written[name] = value;
    }
  }

  setOnScreen(onScreen) {
    this.onScreen = onScreen;
    this.cast(this.latest);
  }
}

// Whether the host draws frames: only for a canvas on screen, in a shown tab, in the window the
// reader is using, and not while the Scene holds still. A road nobody watches costs no frame.
export function drawsFrames(environment) {
  return Boolean(
    environment.onScreen &&
      !environment.pageHidden &&
      environment.focused &&
      !holdsStill(environment),
  );
}

// Whether a frame is due at `now`, the last having been drawn at `last`, for a Scene capped at
// `fps`. A little early counts, so a 30 fps cap lands on every other tick of a 60 Hz display
// rather than slipping to every third.
export function frameDue(now, last, fps) {
  return !fps || now - last >= 1000 / fps - 2;
}

// A phone, and the screens where the road holds still: the stylesheet turns the signs into a
// plain list under the same query.
export const PHONE = "(max-width: 860px)";
export const STILL_MEDIA = `${PHONE}, (prefers-reduced-motion: reduce)`;

// The theme's roles, as the page's stylesheet names them on the element carrying `data-theme`.
const ROLES = {
  ground: "--bg",
  text: "--fg",
  accent: "--accent",
  muted: "--muted",
  keyword: "--keyword",
};

// The host: one canvas, one Scene. It sizes the canvas to the Scene's pixels, reads the theme off
// the page, keeps the Scene's clock, and draws only while `drawsFrames` allows, capped at the
// Scene's `fps`. A Scene that declares `glass: "crt"` is shown through the CRT glass below.
export class SceneHost {
  // `held` holds the Scene still and leaves out its glass: one calm frame, as a release page's
  // header shows it. `beat` reads the radio's beat, 0 to 1, for each frame. `lit` is the element
  // the Scene's light falls on, as `lightProperties` gives it, while that element is on screen;
  // `lightUp` adds another.
  constructor(canvas, scene, { chosen = {}, held = false, beat = () => 0, lit = null } = {}) {
    this.canvas = canvas;
    // A Scene's canvas is small and redrawn every frame; kept in memory rather than on the GPU,
    // Firefox draws it several times faster.
    this.context = canvas.getContext("2d", { willReadFrequently: true });
    this.scene = scene;
    this.chosen = chosen;
    this.held = held;
    this.beat = beat;
    this.time = 0;
    this.navigating = 0;
    this.parked = false;
    this.hush = 0;
    this.rise = 1;
    this.streak = 0;
    this.state = {};
    this.frame = 0;
    this.last = -Infinity;
    this.onScreen = false;
    this.reduced = matchMedia("(prefers-reduced-motion: reduce)");
    this.phone = matchMedia(PHONE);
    this.glass = scene.glass === "crt" && !held ? new CrtGlass(canvas, scene) : null;
    this.lights = lit ? [new SceneLight(lit)] : [];

    new IntersectionObserver((entries) => {
      this.onScreen = entries[entries.length - 1].isIntersecting;
      this.update();
    }).observe(canvas);
    new ResizeObserver(() => this.layout()).observe(canvas);
    // A theme change is an attribute on the root; the Scene starts over in the new palette.
    new MutationObserver(() => this.layout(true)).observe(document.documentElement, {
      attributes: true,
      attributeFilter: ["data-theme"],
    });
    const update = () => this.update();
    document.addEventListener("visibilitychange", update);
    addEventListener("focus", update);
    addEventListener("blur", update);
    for (const query of [this.reduced, this.phone]) {
      query.addEventListener("change", () => this.layout(true));
    }
    this.layout(true);
  }

  // The theme and the canvas's size are read at layout only: a frame that read them would make
  // the browser work out the page's style and layout again, thirty times a second.
  measure() {
    const style = getComputedStyle(this.canvas);
    const rect = this.canvas.getBoundingClientRect();
    this.measured = {
      palette: Object.fromEntries(
        Object.entries(ROLES).map(([role, name]) => [role, style.getPropertyValue(name)]),
      ),
      width: rect.width,
      height: rect.height,
    };
  }

  environment() {
    return {
      ...this.measured,
      time: this.time,
      navigating: this.navigating,
      parked: this.parked,
      hush: this.hush,
      rise: this.rise,
      streak: this.streak,
      beat: this.beat(),
      reducedMotion: this.reduced.matches,
      phone: this.phone.matches,
      chosen: this.chosen,
      held: this.held,
      onScreen: this.onScreen,
      pageHidden: document.hidden,
      focused: document.hasFocus(),
    };
  }

  // A new size, theme or stillness: the canvas is sized again, the Scene's caches go, and it
  // draws once so a still Scene shows its frame.
  layout(restart) {
    this.measure();
    const input = sceneInput(this.scene, this.environment());
    if (this.canvas.width !== input.width || this.canvas.height !== input.height) {
      this.canvas.width = input.width;
      this.canvas.height = input.height;
      restart = true;
    }
    if (restart) this.state = {};
    this.glass?.layout(input);
    this.draw();
    this.update();
  }

  // Whether the road stands still while the rest of the Scene moves: the page parks it on the
  // Start page until the journey starts.
  setParked(value) {
    this.parked = value;
  }

  // How far the stars streak into speed lines, 0 to 1.
  setStreak(value) {
    this.streak = value;
  }

  // How high the sun stands, 0 to 1 of its height.
  setRise(value) {
    this.rise = value;
  }

  // The hush before the drop, 0 to 1, in which the shooting stars fade.
  setHush(value) {
    this.hush = value;
  }

  // How hard the reader is navigating, 0 to 1: the page sets it from the scroll's speed.
  setNavigating(value) {
    this.navigating = value;
  }

  draw() {
    const environment = this.environment();
    const input = { ...sceneInput(this.scene, environment), state: this.state };
    this.scene.draw(this.context, input);
    this.glass?.draw(this.context, input);
    if (this.lights.length) {
      const properties = lightProperties(this.scene, input);
      for (const light of this.lights) light.cast(properties);
    }
  }

  // The Scene's light on one more element, as on the first, while it is on screen.
  lightUp(element) {
    this.lights.push(new SceneLight(element));
  }

  update() {
    if (!drawsFrames(this.environment())) {
      cancelAnimationFrame(this.frame);
      this.frame = 0;
      return;
    }
    if (this.frame) return;
    let previous = performance.now();
    const tick = (now) => {
      if (frameDue(now, this.last, this.scene.fps)) {
        this.time += Math.min(0.1, (now - previous) / 1000);
        previous = now;
        this.last = now;
        this.draw();
      }
      this.frame = drawsFrames(this.environment()) ? requestAnimationFrame(tick) : 0;
    };
    this.frame = requestAnimationFrame(tick);
  }
}

// The glass's static layers from the Scene's `crt` amounts, as styles, for a Scene whose rows are
// `row` CSS pixels tall on a screen of `ratio` device pixels to the CSS pixel:
//
// - `bloom`, the bloom's opacity;
// - `lift`, a filter that brightens the picture by the beam's and the mask's lifts, before they
//   darken it;
// - `scan`, one pass of the beam to each of the Scene's rows, darkest between rows. A tube's beam
//   widens over a bright row and thins over a dark one; one layer for the whole picture cannot
//   follow each row, so every line is as wide as the beam half way between its narrowest and
//   widest;
// - `mask`, an aperture grille of red, green and blue stripes one device pixel wide, multiplied
//   over the picture;
// - `halo`, the halation: how many times smaller than the Scene its averaged copy is, so that a
//   pixel of it is `reach` CSS pixels, the blur that spreads it `spacing` CSS pixels, and how much
//   of it is screened over the picture;
// - `vignette`, clear to `vignette.clear` of the way out and `vignette.shade` dark at the corners.
//
// A vector Scene had no raster, so it has no scanlines and no mask.
export function glassLayers(crt, { pitch, row = pitch, ratio = 1, vector = false }) {
  const percent = (share) => `${Math.round(share * 100)}%`;
  const px = (value) => `${Number(value.toFixed(3))}px`;
  const layers = {
    bloom: String(crt.bloom.opacity),
    halo: {
      scale: crt.halation.reach / pitch,
      blur: px(crt.halation.spacing),
      opacity: String(crt.halation.strength),
    },
    vignette: `radial-gradient(ellipse at center, transparent ${percent(crt.vignette.clear)}, rgb(0 0 0 / ${percent(crt.vignette.shade)}) 100%)`,
  };
  if (vector) return { ...layers, lift: "none", scan: "none", mask: "none" };
  const { shade, narrowest, widest, falloff, lift } = crt.scanlines;
  const width = (narrowest + widest) / 2;
  const steps = 8;
  const scan = Array.from({ length: steps + 1 }, (_, i) => {
    const across = Math.abs(i / steps - 0.5) * 2;
    const beam = Math.exp(-((across / width) ** 2) * falloff);
    const alpha = Math.round(shade * (1 - beam) * 1000) / 10;
    return `rgb(0 0 0 / ${alpha}%) ${px((row * i) / steps)}`;
  });
  const dim = Math.round(crt.mask.dim * 255);
  const stripe = 1 / ratio;
  return {
    ...layers,
    lift: `brightness(${Number((lift * crt.mask.lift).toFixed(3))})`,
    scan: `repeating-linear-gradient(${scan.join(", ")})`,
    mask: `repeating-linear-gradient(90deg, rgb(255 ${dim} ${dim}) 0 ${px(stripe)}, rgb(${dim} 255 ${dim}) ${px(stripe)} ${px(2 * stripe)}, rgb(${dim} ${dim} 255) ${px(2 * stripe)} ${px(3 * stripe)})`,
  };
}

// How much of a channel's `value`, 0 to 1, the halation counts: only what is brighter than its
// threshold, rescaled to 0 to 1.
export function halation(value, crt) {
  const { threshold } = crt.halation;
  return Math.max(value - threshold, 0) / (1 - threshold);
}

// The tint every Omarchy theme is rendered with, which the fallback hue is drawn at: the site's
// themes name none.
const TINT = { saturation: 0.34, lightness: 0.66 };

function hsl([red, green, blue]) {
  const [r, g, b] = [red / 255, green / 255, blue / 255];
  const high = Math.max(r, g, b);
  const low = Math.min(r, g, b);
  const lightness = (high + low) / 2;
  const chroma = high - low;
  if (chroma === 0) return { hue: -1, saturation: 0, lightness };
  const saturation = chroma / (1 - Math.abs(2 * lightness - 1));
  const sector =
    high === r
      ? ((g - b) / chroma + 6) % 6
      : high === g
        ? (b - r) / chroma + 2
        : (r - g) / chroma + 4;
  return { hue: sector / 6, saturation, lightness };
}

function fromHsl(hue, saturation, lightness) {
  const chroma = (1 - Math.abs(2 * lightness - 1)) * saturation;
  const channel = (n) => {
    const k = (n + hue * 12) % 12;
    return lightness - (chroma / 2) * Math.max(-1, Math.min(k - 3, 9 - k, 1));
  };
  return [channel(0), channel(8), channel(4)].map((value) => Math.round(value * 255));
}

// The grade's second hue, as the browser takes it (src/ui/SceneHost.qml): the theme's keyword
// colour, or, where it would add nothing, its hue within 30 degrees of the accent's or itself
// nearly grey, or where the page has none, the accent's hue turned half way round at the tint.
export function gradeHue(palette, tint = TINT) {
  const accent = hsl(palette.accent);
  if (palette.keyword) {
    const keyword = hsl(palette.keyword);
    const apart = Math.abs(keyword.hue - accent.hue);
    if (keyword.saturation >= 0.15 && (accent.hue < 0 || Math.min(apart, 1 - apart) >= 30 / 360)) {
      return palette.keyword;
    }
  }
  return fromHsl((Math.max(0, accent.hue) + 0.5) % 1, tint.saturation, tint.lightness);
}

function smoothstep(from, to, value) {
  const t = Math.max(0, Math.min(1, (value - from) / (to - from)));
  return t * t * (3 - 2 * t);
}

function luma([red, green, blue]) {
  return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

// A colour, 0 to 1, graded toward `hue` as a sunset at `y`, 0 to 1 down the frame: the bright parts
// take the hue at their own brightness, more of them lower in the frame, by the `crt` block's
// `grade` amounts. A channel past full clips, as the screen's does. A Scene that declares no grade
// keeps its colours.
export function grade(colour, y, crt, hue) {
  if (!crt.grade) return colour;
  const { top, depth, highlights, midtones } = crt.grade;
  const level = luma(colour);
  const tone = hue.map((channel) => channel / 255);
  const tinted = tone.map((channel) => channel * (level / Math.max(luma(tone), 0.05)));
  const down = Math.max(0, Math.min(1, (y - top) / depth));
  const amount = Math.max(
    smoothstep(highlights.from, highlights.to, level) * highlights.strength * down,
    smoothstep(midtones.from, midtones.to, level) * midtones.strength * down * down,
  );
  return colour.map((channel, i) => Math.min(1, channel + (tinted[i] - channel) * amount));
}

// A whole picture of `width` by `height` RGBA pixels, graded in place as `grade` grades a colour,
// every frame. The same sums, without a colour or a call per pixel: the glass grades every pixel of
// the Scene thirty times a second, and a night picture is mostly too dark for either range to
// touch, so those pixels are passed over.
export function gradePicture(data, width, height, crt, hue) {
  if (!crt.grade) return;
  const { top, depth, highlights, midtones } = crt.grade;
  const [toneRed, toneGreen, toneBlue] = hue.map((channel) => channel / 255);
  const toTone = 1 / Math.max(luma([toneRed, toneGreen, toneBlue]), 0.05);
  const darkest = Math.min(highlights.from, midtones.from);
  for (let row = 0; row < height; ++row) {
    const y = (row + 0.5) / height;
    // Above the sunset's top nothing takes the hue.
    if (y <= top) continue;
    const down = Math.max(0, Math.min(1, (y - top) / depth));
    const high = highlights.strength * down;
    const mid = midtones.strength * down * down;
    for (let at = row * width * 4, end = at + width * 4; at < end; at += 4) {
      const red = data[at] / 255;
      const green = data[at + 1] / 255;
      const blue = data[at + 2] / 255;
      const level = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
      if (level <= darkest) continue;
      const amount = Math.max(
        smoothstep(highlights.from, highlights.to, level) * high,
        smoothstep(midtones.from, midtones.to, level) * mid,
      );
      const scale = level * toTone;
      data[at] = Math.min(1, red + (toneRed * scale - red) * amount) * 255;
      data[at + 1] = Math.min(1, green + (toneGreen * scale - green) * amount) * 255;
      data[at + 2] = Math.min(1, blue + (toneBlue * scale - blue) * amount) * 255;
    }
  }
}

// Where the refresh band is at `time` on a picture `height` of the Scene's pixels tall, how far it
// reaches and how strong it is, and how much the flicker darkens: never less than its least, and
// never a flash. A Scene that declares no band, as a vector one, has none.
export function glassRoll(crt, { time, height }) {
  const { band, flicker } = crt;
  const [fast, slow] = flicker.frequencies;
  return {
    y: band ? ((time / band.every) % 1) * height * band.travel + height * band.from : 0,
    reach: band ? Math.max(band.reachAtLeast, height * band.reach) : 1,
    strength: band ? band.strength : 0,
    flicker:
      flicker.least + flicker.range * Math.abs(Math.sin(time * fast) * Math.sin(time * slow)),
  };
}

// The CRT a Scene can declare, composited by the browser rather than drawn: the Scene's small
// canvas scaled up pixelated and lifted, a tiny copy of it scaled up smooth for the bloom, static
// scanline, mask and vignette layers, and the halation, a copy of the picture averaged down and cut
// at its threshold, blurred and screened over it. Only the rolling band, the flicker and a vector
// Scene's trail are drawn, into the small picture itself, and the halation's copy is smaller still,
// so a frame costs a few hundred thousand pixels however large the window.
class CrtGlass {
  constructor(canvas, scene) {
    this.crt = scene.crt;
    this.pitch = scene.pitch || 1;
    this.vector = scene.screen === "vector";
    this.canvas = canvas;
    this.frame = canvas.parentElement;
    this.frame.classList.add("crt");
    const layer = (tag, name) => {
      const element = document.createElement(tag);
      element.className = name;
      element.setAttribute("aria-hidden", "true");
      this.frame.append(element);
      return element;
    };
    this.bloom = layer("canvas", "crt__bloom");
    this.scan = layer("div", "crt__scan");
    this.halo = layer("canvas", "crt__halo");
    this.mask = layer("div", "crt__mask");
    this.vignette = layer("div", "crt__vignette");
    // A vector screen's phosphor persisted: the last frame, which the next keeps where it is the
    // brighter.
    this.trail = this.vector ? document.createElement("canvas") : null;
  }

  layout(input) {
    this.hue = gradeHue(input.palette);
    const rect = this.canvas.getBoundingClientRect();
    const layers = glassLayers(this.crt, {
      pitch: this.pitch,
      row: rect.height / input.height || this.pitch,
      ratio: devicePixelRatio || 1,
      vector: this.vector,
    });
    this.bloom.style.opacity = layers.bloom;
    this.canvas.style.filter = this.bloom.style.filter = layers.lift;
    this.scan.style.background = layers.scan;
    this.mask.style.background = layers.mask;
    this.vignette.style.background = layers.vignette;
    this.halo.style.filter = `blur(${layers.halo.blur})`;
    this.halo.style.opacity = layers.halo.opacity;
    this.bloom.width = Math.max(1, Math.ceil(input.width / this.crt.bloom.scale));
    this.bloom.height = Math.max(1, Math.ceil(input.height / this.crt.bloom.scale));
    // The halation's copy is made by halving the picture until it is small enough: a pixel of
    // each half is the average of four, where one step down would pick points from the picture
    // and put the glow off what casts it.
    this.halving = [];
    for (let scale = 2; scale / 2 < layers.halo.scale; scale *= 2) {
      const step = document.createElement("canvas");
      step.width = Math.max(1, Math.ceil(input.width / Math.min(scale, layers.halo.scale)));
      step.height = Math.max(1, Math.ceil(input.height / Math.min(scale, layers.halo.scale)));
      this.halving.push(step);
    }
    const smallest = this.halving[this.halving.length - 1] || this.canvas;
    // The smallest is read back every frame, so it is kept in memory rather than on the GPU.
    smallest.getContext("2d", { willReadFrequently: true });
    this.halo.width = smallest.width;
    this.halo.height = smallest.height;
    if (this.trail) {
      this.trail.width = input.width;
      this.trail.height = input.height;
    }
  }

  draw(context, input) {
    if (!input.reducedMotion) {
      const { width } = input;
      // The refresh band rolls down the picture, and the flicker darkens it a little.
      const { y, reach, strength, flicker } = glassRoll(this.crt, input);
      if (strength > 0) {
        const band = context.createLinearGradient(0, y - reach, 0, y + reach);
        band.addColorStop(0, "rgba(255, 255, 255, 0)");
        band.addColorStop(0.5, `rgba(${input.palette.text.join(", ")}, ${strength})`);
        band.addColorStop(1, "rgba(255, 255, 255, 0)");
        context.fillStyle = band;
        context.fillRect(0, y - reach, width, reach * 2);
      }
      context.fillStyle = `rgba(0, 0, 0, ${flicker.toFixed(3)})`;
      context.fillRect(0, 0, width, input.height);
      if (this.trail) this.persist(context);
    }
    // The halation is cast by the picture before its grade, and the bloom after, as the
    // browser's glass draws them.
    this.halate(context);
    this.grade(context, input);
    this.bloom
      .getContext("2d")
      .drawImage(context.canvas, 0, 0, this.bloom.width, this.bloom.height);
  }

  // The grade toward the second hue, over the Scene's picture. A Scene that declares none is not
  // graded, as in the browser.
  grade(context, input) {
    if (!this.crt.grade) return;
    const { width, height } = input;
    const image = context.getImageData(0, 0, width, height);
    gradePicture(image.data, width, height, this.crt, this.hue);
    context.putImageData(image, 0, 0);
  }

  // Each frame keeps the brighter of the picture and the last frame, faded by `trail.fade`.
  persist(context) {
    const trail = this.trail.getContext("2d");
    trail.globalCompositeOperation = "source-over";
    trail.fillStyle = `rgba(0, 0, 0, ${1 - this.crt.trail.fade})`;
    trail.fillRect(0, 0, this.trail.width, this.trail.height);
    context.save();
    context.globalCompositeOperation = "lighten";
    context.drawImage(this.trail, 0, 0);
    context.restore();
    trail.globalCompositeOperation = "copy";
    trail.drawImage(context.canvas, 0, 0);
  }

  halate(context) {
    let source = context.canvas;
    for (const step of this.halving) {
      step.getContext("2d").drawImage(source, 0, 0, step.width, step.height);
      source = step;
    }
    const image = source.getContext("2d").getImageData(0, 0, source.width, source.height);
    for (let i = 0; i < image.data.length; i += 4) {
      for (let channel = 0; channel < 3; ++channel) {
        image.data[i + channel] = 255 * halation(image.data[i + channel] / 255, this.crt);
      }
    }
    this.halo.getContext("2d").putImageData(image, 0, 0);
  }
}
