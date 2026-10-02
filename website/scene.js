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
  const palette = Object.fromEntries(
    Object.entries(environment.palette).map(([role, colour]) => [role, rgb(colour)]),
  );
  return {
    width: Math.max(1, Math.ceil(environment.width / pitch)),
    height: Math.max(1, Math.ceil(environment.height / pitch)),
    pitch,
    time: environment.time || 0,
    navigating: still ? 0 : Math.max(0, Math.min(1, environment.navigating || 0)),
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
const ROLES = { ground: "--bg", text: "--fg", accent: "--accent", muted: "--muted" };

// The host: one canvas, one Scene. It sizes the canvas to the Scene's pixels, reads the theme off
// the page, keeps the Scene's clock, and draws only while `drawsFrames` allows, capped at the
// Scene's `fps`. A Scene that declares `glass: "crt"` is shown through the CRT glass below.
export class SceneHost {
  // `held` holds the Scene still and leaves out its glass: one calm frame, as a release page's
  // header shows it. `beat` reads the radio's beat, 0 to 1, for each frame. `lit` is the element
  // the Scene's light falls on, as `lightProperties` gives it, while that element is on screen.
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
    this.state = {};
    this.frame = 0;
    this.last = -Infinity;
    this.onScreen = false;
    this.reduced = matchMedia("(prefers-reduced-motion: reduce)");
    this.phone = matchMedia(PHONE);
    this.glass = scene.glass === "crt" && !held ? new CrtGlass(canvas, scene.crt) : null;
    this.light = lit && new SceneLight(lit);

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

  // How hard the reader is navigating, 0 to 1: the page sets it from the scroll's speed.
  setNavigating(value) {
    this.navigating = value;
  }

  draw() {
    const environment = this.environment();
    const input = { ...sceneInput(this.scene, environment), state: this.state };
    this.scene.draw(this.context, input);
    this.glass?.draw(this.context, input);
    this.light?.cast(lightProperties(this.scene, input));
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

// The glass's static layers from the Scene's `crt` amounts, as styles: the bloom's opacity, one
// darker line in every `scanlines.every` CSS pixels, and a vignette clear to `vignette.clear` of
// the way out and `vignette.shade` dark at the corners.
export function glassLayers(crt) {
  const percent = (share) => `${Math.round(share * 100)}%`;
  const { every, shade } = crt.scanlines;
  return {
    bloom: String(crt.bloom.opacity),
    scan: `repeating-linear-gradient(transparent 0 ${every - 1}px, rgb(0 0 0 / ${percent(shade)}) ${every - 1}px ${every}px)`,
    vignette: `radial-gradient(ellipse at center, transparent ${percent(crt.vignette.clear)}, rgb(0 0 0 / ${percent(crt.vignette.shade)}) 100%)`,
  };
}

// Where the refresh band is at `time` on a picture `height` of the Scene's pixels tall, how far it
// reaches and how strong it is, and how much the flicker darkens: never less than its least, and
// never a flash.
export function glassRoll(crt, { time, height }) {
  const { band, flicker } = crt;
  const [fast, slow] = flicker.frequencies;
  return {
    y: ((time / band.every) % 1) * height * band.travel + height * band.from,
    reach: Math.max(band.reachAtLeast, height * band.reach),
    strength: band.strength,
    flicker:
      flicker.least + flicker.range * Math.abs(Math.sin(time * fast) * Math.sin(time * slow)),
  };
}

// The CRT a Scene can declare, composited by the browser rather than drawn: the Scene's small
// canvas scaled up pixelated, a tiny copy of it scaled up smooth for the bloom, and static
// scanline and vignette layers. Only the rolling band and the flicker are drawn, into the small
// picture itself, so a frame costs a few hundred thousand pixels however large the window.
class CrtGlass {
  constructor(canvas, crt) {
    this.crt = crt;
    const layers = glassLayers(crt);
    const frame = canvas.parentElement;
    frame.classList.add("crt");
    this.bloom = document.createElement("canvas");
    this.bloom.className = "crt__bloom";
    this.bloom.setAttribute("aria-hidden", "true");
    this.bloom.style.opacity = layers.bloom;
    frame.append(this.bloom);
    for (const [name, background] of [
      ["crt__scan", layers.scan],
      ["crt__vignette", layers.vignette],
    ]) {
      const layer = document.createElement("div");
      layer.className = name;
      layer.style.background = background;
      frame.append(layer);
    }
  }

  layout(input) {
    this.bloom.width = Math.max(1, Math.ceil(input.width / this.crt.bloom.scale));
    this.bloom.height = Math.max(1, Math.ceil(input.height / this.crt.bloom.scale));
  }

  draw(context, input) {
    if (!input.reducedMotion) {
      const { width } = input;
      // The refresh band rolls down the picture, and the flicker darkens it a little.
      const { y, reach, strength, flicker } = glassRoll(this.crt, input);
      const band = context.createLinearGradient(0, y - reach, 0, y + reach);
      band.addColorStop(0, "rgba(255, 255, 255, 0)");
      band.addColorStop(0.5, `rgba(${input.palette.text.join(", ")}, ${strength})`);
      band.addColorStop(1, "rgba(255, 255, 255, 0)");
      context.fillStyle = band;
      context.fillRect(0, y - reach, width, reach * 2);
      context.fillStyle = `rgba(0, 0, 0, ${flicker.toFixed(3)})`;
      context.fillRect(0, 0, width, input.height);
    }
    this.bloom
      .getContext("2d")
      .drawImage(context.canvas, 0, 0, this.bloom.width, this.bloom.height);
  }
}
