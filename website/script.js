// Omaweb website behaviour. All of it optional: the page renders and reads
// correctly with this file blocked -- index.html names the theme every themed
// asset starts on, each shot is a plain link to its full screenshot, the
// first shot is the one on show, the boot console is printed in full, the
// canvas behind the hero is decoration, and the nav is in view until the
// script folds it behind its button.
//
//   1. The scene behind the hero: the drawing the screenshots' wallpaper is
//      made of, in the active palette, falling slowly; and on the landing
//      page a horizon under it, with a banded sun and a grid running out
//      towards the reader.
//   2. Theme switching. One palette drives the page and the generated
//      screenshots, so picking a theme restyles both at once, and a pick
//      sweeps the new palette out of the button pressed. The choice is saved
//      locally, and a `theme` query parameter lets a shared URL choose its
//      palette. In Omaweb the reader's own theme is one of the choices, and
//      the one the page starts on.
//   3. The menu a phone gets in place of the row of links in the header.
//   4. The walk: which shot the pinned frame shows, which is the one beside
//      the step nearest the middle of the window.
//   5. Opening a screenshot in a viewer instead of navigating to the file.
//   6. The boot console printing its lines one at a time.
//   7. The browser's own bindings, on its website: F for link hints and T
//      for the next theme.

(function () {
  "use strict";

  var reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");

  // ---------------------------------------------------------------- scene
  var scene = document.querySelector(".t-scene");
  var sceneContext = scene && scene.getContext && scene.getContext("2d");
  var floor = document.querySelector(".t-hero__floor");
  var bootConsole = document.querySelector(".t-console");

  function hash01() {
    var state = 0x9e3779b9;
    for (var index = 0; index < arguments.length; index += 1) {
      state = Math.imul(state ^ arguments[index], 0x85ebca6b);
      state ^= state >>> 13;
      state = Math.imul(state, 0xc2b2ae35);
      state ^= state >>> 16;
    }
    return (state >>> 0) / 4294967296;
  }

  // A palette value is whatever a stylesheet wrote: the generated themes are
  // hex, and the one Omaweb hands over is `rgb(r g b)`. A canvas normalises
  // any colour it is given to `#rrggbb`, so it is read through one rather
  // than parsed here.
  var colorProbe = document.createElement("canvas").getContext("2d");

  function parseColor(value) {
    var digits = value.trim();
    if (colorProbe) {
      colorProbe.fillStyle = "#000";
      colorProbe.fillStyle = digits;
      digits = colorProbe.fillStyle;
    }
    digits = digits.replace("#", "");
    if (digits.length === 3) {
      digits = digits.replace(/./g, function (digit) {
        return digit + digit;
      });
    }
    return [0, 2, 4].map(function (at) {
      return parseInt(digits.slice(at, at + 2), 16);
    });
  }

  function mix(first, second, amount) {
    return first.map(function (channel, index) {
      return channel + (second[index] - channel) * amount;
    });
  }

  function css(color, alpha) {
    var channels = color.map(Math.round).join(",");
    return alpha === undefined ? "rgb(" + channels + ")" : "rgba(" + channels + "," + alpha + ")";
  }

  var palette = null;

  function readPalette() {
    var style = getComputedStyle(document.body);
    var background = parseColor(style.getPropertyValue("--bg"));
    var foreground = parseColor(style.getPropertyValue("--fg"));
    var accent = parseColor(style.getPropertyValue("--accent"));
    var urgent = parseColor(style.getPropertyValue("--urgent"));
    palette = {
      bg: background,
      fg: foreground,
      accent: accent,
      // The stylesheet's `--hot`: urgent lifted towards the accent.
      hot: mix(urgent, accent, 0.3),
      rain: [
        mix(background, accent, 0.22),
        mix(background, accent, 0.42),
        mix(background, accent, 0.68),
        accent,
        mix(accent, foreground, 0.55),
      ].map(function (color) {
        return css(color);
      }),
    };
  }

  // Square cells on a coarse grid, falling in columns from the top edge and
  // thinning out as they fall, in four tints of the accent. The same drawing
  // as `wallpaper()` in scripts/build_website_themes.py, and a change to one
  // wants the same change to the other; only the hash differs, since this one
  // has no 64-bit integers to mix. Time enters as a per-column row offset
  // into the hash, so the pattern slides down each column while the
  // brightness stays anchored to the top, which is what falling looks like.
  function paintRain(context, width, height, time) {
    context.clearRect(0, 0, width, height);
    var pitch = Math.max(10, Math.round(width / 100));
    var cell = Math.round(pitch * 0.58);
    var inset = Math.floor((pitch - cell) / 2);
    var columns = Math.floor(width / pitch) + 1;
    var rows = Math.floor(height / pitch) + 1;

    for (var column = 0; column < columns; column += 1) {
      if (hash01(2, column) < 0.12) continue;
      var noise =
        (0.55 * (hash01(1, column) + hash01(1, column + 1) + hash01(1, column + 2))) / 3 +
        0.45 * hash01(1, column + 1);
      var length = 0.06 + 0.8 * Math.pow(noise, 1.8);
      var streak = hash01(7, column) < 0.25;
      if (streak) length = Math.max(length, 0.55 + 0.45 * hash01(9, column));
      var fall = Math.max(length * rows, 1);
      var run = 1 + Math.floor(hash01(8, column) * 3);
      // Each column falls at its own pace, between one and three cells a
      // second, so the sheet never moves as one.
      var offset = Math.floor((time / 1000) * (1 + 2 * hash01(10, column)));

      for (var row = 0; row < rows; row += 1) {
        var depth = row / fall;
        if (depth > 1.15) break;
        depth = Math.min(1, depth);
        var survive = 1 - Math.pow(depth, 0.9) * 0.9;
        if (streak && depth < 0.75) survive = 1;
        var slid = row - offset;
        if (hash01(3, column, Math.floor(slid / run)) > survive) continue;
        var brightness = Math.pow(1 - depth, 1.1) * (0.65 + 0.5 * hash01(4, column, slid));
        var level = Math.min(3, Math.floor(brightness * 4));
        if (brightness < 0.08) level = 0;
        if (row === 0 && hash01(5, column, slid) < 0.2) level = 4;
        context.fillStyle = palette.rain[level];
        context.fillRect(column * pitch + inset, row * pitch + inset, cell, cell);
      }
    }
  }

  // The horizon. `layout` measures where it goes; everything else is drawn
  // fresh each frame from that and the palette.
  var horizonMode = Boolean(floor);
  var rainLayer = document.createElement("canvas");
  var rainLayerContext = rainLayer.getContext("2d");
  var view = { width: 0, height: 0, horizon: 0, sunX: 0, sunRadius: 0, ratio: 1 };

  function layout() {
    var ratio = Math.min(devicePixelRatio || 1, horizonMode ? 1.5 : 2);
    var width = scene.clientWidth;
    var height = scene.clientHeight;
    if (horizonMode) {
      // The canvas runs from the top of the page to the foot of the floor,
      // and the horizon is the floor's top edge.
      var box = floor.getBoundingClientRect();
      height = Math.round(box.bottom + scrollY);
      scene.style.height = height + "px";
      view.horizon = Math.round(box.top + scrollY);
      // The sun sets behind the console where the two columns stand side by
      // side, and in the middle of the floor where they stack.
      var panel = bootConsole && bootConsole.getBoundingClientRect();
      var beside = panel && panel.left > width / 2;
      view.sunX = beside ? panel.left + panel.width / 2 : width / 2;
      view.sunRadius = beside
        ? Math.min(width * 0.15, view.horizon * 0.3, 230)
        : Math.min(width * 0.3, (view.horizon - (panel ? panel.bottom + scrollY : 0)) * 0.9, 150);
      view.sunRadius = Math.max(view.sunRadius, 60);
    }
    view.width = width;
    view.height = height;
    view.ratio = ratio;
    if (scene.width !== Math.round(width * ratio) || scene.height !== Math.round(height * ratio)) {
      scene.width = Math.round(width * ratio);
      scene.height = Math.round(height * ratio);
    }
    var rainHeight = horizonMode ? view.horizon : height;
    rainLayer.width = Math.round(width * ratio);
    rainLayer.height = Math.max(1, Math.round(rainHeight * ratio));
  }

  // A low ridge along the horizon, so the sun sets behind something. Value
  // noise from the same hash, two octaves, fixed for a given width.
  function ridgeAt(x) {
    function octave(scale, seed) {
      var at = x / scale;
      var left = Math.floor(at);
      var t = at - left;
      t = t * t * (3 - 2 * t);
      return hash01(seed, left) * (1 - t) + hash01(seed, left + 1) * t;
    }
    return 0.65 * octave(90, 21) + 0.35 * octave(28, 22);
  }

  function paintHorizon(context, time) {
    var width = view.width;
    var height = view.height;
    var horizon = view.horizon;
    var floorHeight = height - horizon;

    context.clearRect(0, 0, width, height);

    // The sky: the rain, fading into a haze of the accent above the horizon.
    context.globalAlpha = 0.34;
    context.drawImage(rainLayer, 0, 0, width, horizon);
    context.globalAlpha = 1;
    var haze = context.createLinearGradient(0, horizon * 0.35, 0, horizon);
    haze.addColorStop(0, css(palette.bg, 0));
    haze.addColorStop(0.75, css(palette.bg, 0.7));
    haze.addColorStop(1, css(mix(palette.bg, palette.accent, 0.18), 0.95));
    context.fillStyle = haze;
    context.fillRect(0, 0, width, horizon);

    // The sun: a glow, then the disc, run from the accent into the hot
    // colour, with bands cut out of its lower half that drift downwards.
    var radius = view.sunRadius;
    var sunX = view.sunX;
    var sunY = horizon - radius * 0.28;
    var glow = context.createRadialGradient(sunX, sunY, radius * 0.6, sunX, sunY, radius * 2.4);
    glow.addColorStop(0, css(palette.accent, 0.28));
    glow.addColorStop(1, css(palette.accent, 0));
    context.fillStyle = glow;
    context.fillRect(sunX - radius * 2.4, sunY - radius * 2.4, radius * 4.8, radius * 4.8);

    context.save();
    context.beginPath();
    context.arc(sunX, sunY, radius, 0, Math.PI * 2);
    context.clip();
    var disc = context.createLinearGradient(0, sunY - radius, 0, sunY + radius * 0.4);
    disc.addColorStop(0, css(mix(palette.accent, palette.fg, 0.55)));
    disc.addColorStop(0.45, css(palette.accent));
    disc.addColorStop(1, css(palette.hot));
    context.fillStyle = disc;
    context.fillRect(sunX - radius, sunY - radius, radius * 2, radius * 2);
    var bands = 9;
    var drift = ((time / 1000) * 0.22) % 1;
    var bandTop = sunY - radius * 0.45;
    var bandSpan = radius * 1.45;
    for (var band = 0; band < bands; band += 1) {
      var along = (band + drift) / bands;
      var bandY = bandTop + along * bandSpan;
      context.clearRect(sunX - radius, bandY, radius * 2, 1 + along * radius * 0.09);
    }
    context.restore();

    // The ridge, in the ground colour with a lit edge.
    var step = Math.max(6, width / 160);
    var peak = Math.min(70, horizon * 0.09);
    context.beginPath();
    context.moveTo(0, horizon);
    for (var x = 0; x <= width + step; x += step) {
      // Lower where the sun is, so the ridge frames it rather than hides it.
      var nearSun = Math.min(1, Math.abs(x - sunX) / (radius * 1.6));
      context.lineTo(x, horizon - ridgeAt(x) * peak * (0.35 + 0.65 * nearSun));
    }
    context.lineTo(width, horizon);
    context.closePath();
    context.fillStyle = css(mix(palette.bg, palette.accent, 0.06));
    context.fill();
    context.strokeStyle = css(palette.accent, 0.45);
    context.lineWidth = 1;
    context.stroke();

    // The floor and its grid, running towards the reader. Vertical lines
    // meet at the vanishing point on the horizon; the cross lines are
    // spaced by a power of their depth so they bunch up in the distance.
    var ground = context.createLinearGradient(0, horizon, 0, height);
    ground.addColorStop(0, css(mix(palette.bg, palette.accent, 0.16)));
    ground.addColorStop(0.3, css(mix(palette.bg, palette.accent, 0.05)));
    ground.addColorStop(1, css(palette.bg));
    context.fillStyle = ground;
    context.fillRect(0, horizon, width, floorHeight);

    var lines = context.createLinearGradient(0, horizon, 0, height);
    lines.addColorStop(0, css(palette.accent, 0.05));
    lines.addColorStop(0.35, css(palette.accent, 0.4));
    lines.addColorStop(1, css(palette.accent, 0.85));
    context.strokeStyle = lines;
    context.lineWidth = 1;
    context.beginPath();
    var vanishX = sunX;
    var spread = Math.max(width, 900) / 9;
    var reach = Math.ceil(width / spread) * 3;
    for (var ray = -reach; ray <= reach; ray += 1) {
      context.moveTo(vanishX + ray * spread * 0.04, horizon);
      context.lineTo(vanishX + ray * spread * 1.6, height);
    }
    var crossings = 14;
    var travel = ((time / 1000) * 0.55) % 1;
    for (var cross = 0; cross < crossings; cross += 1) {
      var depth = (cross + travel) / crossings;
      var y = horizon + floorHeight * Math.pow(depth, 2.4);
      context.moveTo(0, y);
      context.lineTo(width, y);
    }
    context.stroke();

    // The horizon itself, lit.
    context.save();
    context.shadowColor = css(palette.accent);
    context.shadowBlur = 16;
    context.fillStyle = css(mix(palette.accent, palette.fg, 0.35));
    context.fillRect(0, horizon - 1, width, 2);
    context.restore();
  }

  var sceneFrame = 0;
  var rainPainted = -Infinity;
  var scenePainted = -Infinity;

  function paintScene(time) {
    var ratio = view.ratio;
    // Cells move whole pitches, so the rain needs drawing only when a column
    // slides, about every 120ms; the horizon moves smoothly and is drawn up
    // to thirty times a second, and not at all once it is scrolled away.
    if (time - rainPainted >= 120) {
      rainLayerContext.setTransform(ratio, 0, 0, ratio, 0, 0);
      paintRain(rainLayerContext, view.width, horizonMode ? view.horizon : view.height, time);
      rainPainted = time;
      if (!horizonMode) {
        sceneContext.setTransform(1, 0, 0, 1, 0, 0);
        sceneContext.clearRect(0, 0, scene.width, scene.height);
        sceneContext.drawImage(rainLayer, 0, 0);
      }
    }
    if (horizonMode && time - scenePainted >= 32 && scrollY < view.height) {
      sceneContext.setTransform(ratio, 0, 0, ratio, 0, 0);
      paintHorizon(sceneContext, time);
      scenePainted = time;
    }
  }

  function stepScene(time) {
    paintScene(time);
    sceneFrame = requestAnimationFrame(stepScene);
  }

  function startScene() {
    cancelAnimationFrame(sceneFrame);
    readPalette();
    layout();
    rainPainted = -Infinity;
    scenePainted = -Infinity;
    if (reducedMotion.matches) paintScene(0);
    else sceneFrame = requestAnimationFrame(stepScene);
  }

  if (sceneContext) {
    if (horizonMode) scene.classList.add("is-horizon");
    startScene();
    addEventListener("resize", startScene);
    reducedMotion.addEventListener("change", startScene);
    // The floor moves when the display face arrives and the title reflows.
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(startScene);
  }

  // ---------------------------------------------------------------- theme
  var themeButtons = [].slice.call(document.querySelectorAll(".t-theme"));
  var THEME_STORAGE_KEY = "omaweb.preview-theme";

  // The screenshots are generated one per theme, under paths that differ
  // only in the theme's name. Each element carries its own path with
  // `{theme}` where that name goes, so adding a themed image is markup and
  // nothing here, and the path is relative to the page it is written on,
  // which is what keeps it working at every depth the site has pages at.
  var themed = [].slice.call(document.querySelectorAll("[data-themed]"));
  var themeColor = document.querySelector('meta[name="theme-color"]');

  // The favicon is left alone. It is a document of its own and sees none of
  // the page's colours, and the one way to redraw it from here, a `data:`
  // URL, is an icon Omaweb's engine never picks up: the site's tab in the
  // browser this is for would have no icon at all. So it is the shipped
  // file in the default palette, in every theme.

  function paintPalette() {
    // The browser chrome around the page follows the palette too, read off
    // the ground the theme just painted rather than listed a second time.
    if (themeColor) {
      themeColor.content = getComputedStyle(document.body).getPropertyValue("--bg").trim();
    }
    if (sceneContext) startScene();
  }

  // Which themes exist, from the `--themes` list `themes.css` is generated
  // with. The switcher buttons are only on the landing page, so asking them
  // would leave every other page unable to tell a stored theme from a stale
  // one: it would fall back to the default and the reader's choice would stop
  // at the first link they followed.
  var themeNames = getComputedStyle(document.documentElement)
    .getPropertyValue("--themes")
    .replace(/["']/g, "")
    .trim()
    .split(/\s+/)
    .filter(Boolean);

  // The reader's own theme, which Omaweb hands a page that asks for it as
  // `--omaweb-*` on the root element. The stylesheet already paints in it
  // wherever it is there; this makes it a theme the picker knows, so it can
  // be left for a preview and come back to. There is no screenshot set in
  // an arbitrary palette, so "own" shows the shipped set whose palette is
  // nearest, measured on the ground, the text and the accent.
  var OWN_THEME = "own";
  var ownPalette = readOwnPalette();
  var nearestShipped = ownPalette ? nearestTheme(ownPalette) : null;
  if (ownPalette) themeNames.unshift(OWN_THEME);

  function readOwnPalette() {
    var style = getComputedStyle(document.documentElement);
    var palette = {};
    var roles = ["bg", "fg", "accent"];
    for (var index = 0; index < roles.length; index += 1) {
      var value = style.getPropertyValue("--omaweb-" + roles[index]).trim();
      if (!value) return null;
      palette[roles[index]] = value;
    }
    return palette;
  }

  // Read off the body under each shipped theme in turn, which is where
  // themes.css defines them; seven style resolutions once, before the page
  // has painted anything the reader would see move.
  function nearestTheme(own) {
    function distance(a, b) {
      var sum = 0;
      for (var index = 0; index < 3; index += 1) sum += Math.pow(a[index] - b[index], 2);
      return sum;
    }
    var wanted = { bg: parseColor(own.bg), fg: parseColor(own.fg), accent: parseColor(own.accent) };
    var was = document.body.dataset.theme;
    var best = null;
    var bestDistance = Infinity;
    themeNames.forEach(function (name) {
      document.body.dataset.theme = name;
      var style = getComputedStyle(document.body);
      var total = 0;
      for (var role in wanted) {
        total += distance(wanted[role], parseColor(style.getPropertyValue("--" + role)));
      }
      if (total < bestDistance) {
        bestDistance = total;
        best = name;
      }
    });
    if (was === undefined) delete document.body.dataset.theme;
    else document.body.dataset.theme = was;
    return best;
  }

  function hasTheme(name) {
    return themeNames.indexOf(name) !== -1;
  }

  function readSavedTheme() {
    try {
      return localStorage.getItem(THEME_STORAGE_KEY);
    } catch (error) {
      return null;
    }
  }

  function saveTheme(name) {
    try {
      localStorage.setItem(THEME_STORAGE_KEY, name);
    } catch (error) {
      // The preview still works when storage is unavailable.
    }
  }

  function setTheme(name, options) {
    if (!hasTheme(name)) return;

    function apply() {
      // No named theme on the body is the reader's own: the stylesheet's
      // ground is `--omaweb-*` wherever Omaweb supplies it, and a named theme
      // outranks that.
      if (name === OWN_THEME) delete document.body.dataset.theme;
      else document.body.dataset.theme = name;
      themeButtons.forEach(function (button) {
        button.setAttribute("aria-pressed", String(button.dataset.theme === name));
      });

      var pictured = name === OWN_THEME ? nearestShipped : name;
      themed.forEach(function (element) {
        var path = element.dataset.themed.replace("{theme}", pictured);
        if (element.tagName === "A") element.href = path;
        else element.src = path;
      });
      // The boot console names the theme on air, by the picker's own label.
      var named = themeButtons.filter(function (button) {
        return button.dataset.theme === name;
      })[0];
      var label = named ? named.textContent.trim() : name;
      [].forEach.call(document.querySelectorAll("[data-theme-name]"), function (node) {
        node.textContent = label;
      });
      paintPalette();
    }

    // A pick made with a button sweeps the new palette out of it: the change
    // runs inside a view transition, and the stylesheet grows the new page as
    // a circle from the point handed over here. Without the API, or with
    // motion reduced, the change is the same and simply immediate.
    var from = options.from;
    if (from && !reducedMotion.matches && typeof document.startViewTransition === "function") {
      var box = from.getBoundingClientRect();
      document.documentElement.style.setProperty("--sweep-x", box.left + box.width / 2 + "px");
      document.documentElement.style.setProperty("--sweep-y", box.top + box.height / 2 + "px");
      // Marked for the sweep's length, and the mark taken off however the
      // transition ends: a second pick skips the first, and `finished`
      // rejects for a skipped one.
      document.documentElement.classList.add("is-sweeping");
      document.startViewTransition(apply).finished.finally(function () {
        document.documentElement.classList.remove("is-sweeping");
      });
    } else {
      apply();
    }

    if (options.save) saveTheme(name);
    // A link can name a shipped theme; the reader's own is not one another
    // reader can be sent to.
    if (options.share) {
      var url = new URL(location.href);
      if (name === OWN_THEME) url.searchParams.delete("theme");
      else url.searchParams.set("theme", name);
      history.replaceState(null, "", url);
    }
  }

  themeButtons.forEach(function (button) {
    button.addEventListener("click", function () {
      if (button.getAttribute("aria-pressed") === "true") return;
      setTheme(button.dataset.theme, { save: true, share: true, from: button });
    });
  });

  // Each button carries its theme's own ground, accent and foreground, read
  // off the body under that theme the way nearestTheme reads them, and
  // written onto the swatch through the CSSOM: the site's policy allows no
  // inline style attribute. The reader's own theme takes its colours from
  // the `--omaweb-*` properties the stylesheet already resolves.
  function paintSwatches() {
    var was = document.body.dataset.theme;
    themeButtons.forEach(function (button) {
      if (button.hidden) return;
      if (button.dataset.theme === OWN_THEME) delete document.body.dataset.theme;
      else document.body.dataset.theme = button.dataset.theme;
      var style = getComputedStyle(document.body);
      var swatch = document.createElement("span");
      swatch.className = "t-theme__swatch";
      swatch.setAttribute("aria-hidden", "true");
      ["bg", "accent", "fg"].forEach(function (role) {
        swatch.appendChild(document.createElement("i"));
        swatch.style.setProperty("--swatch-" + role, style.getPropertyValue("--" + role));
      });
      button.insertBefore(swatch, button.firstChild);
    });
    if (was === undefined) delete document.body.dataset.theme;
    else document.body.dataset.theme = was;
  }

  // A shared link names its palette on purpose and wins. Otherwise the
  // reader's own theme wins over a saved preview: the preview was picked to
  // see a theme, and their own theme is the one they see everywhere else.
  themeButtons.forEach(function (button) {
    if (button.dataset.theme === OWN_THEME) button.hidden = !ownPalette;
  });
  paintSwatches();
  var queryTheme = new URLSearchParams(location.search).get("theme");
  var savedTheme = readSavedTheme();
  var initialTheme = hasTheme(queryTheme) ? queryTheme : ownPalette ? OWN_THEME : savedTheme;
  if (hasTheme(initialTheme)) setTheme(initialTheme, { save: Boolean(queryTheme), share: false });

  // ---------------------------------------------------------------- copy
  // ---------------------------------------------------------------- menu
  // On a phone the nav folds behind a button. The stylesheet does the
  // folding, keyed on the header's `data-menu` mark and the button's
  // expanded state, so this only flips them: on the button, on a link (the
  // install one stays on the page, so nothing else would close it), and on
  // Escape. Above the breakpoint the stylesheet ignores both, so the state is
  // left alone across resizes.
  var top = document.querySelector(".t-top");
  var toggle = top && top.querySelector(".t-nav-toggle");
  var sections = toggle && document.getElementById(toggle.getAttribute("aria-controls"));

  if (toggle && sections) {
    function setMenu(open) {
      toggle.setAttribute("aria-expanded", String(open));
      top.setAttribute("data-menu", open ? "open" : "closed");
    }

    setMenu(false);
    toggle.hidden = false;
    toggle.addEventListener("click", function () {
      setMenu(toggle.getAttribute("aria-expanded") !== "true");
    });
    sections.addEventListener("click", function (event) {
      if (event.target.closest("a")) setMenu(false);
    });
    addEventListener("keydown", function (event) {
      if (event.key !== "Escape" || toggle.getAttribute("aria-expanded") !== "true") return;
      setMenu(false);
      toggle.focus();
    });
  }

  // The button copies the command beside it, not the prompt or the cursor,
  // which are drawn for the reader and would be wrong in a shell. Without
  // the clipboard API, which needs a secure context, the button goes away
  // and the text is still there to select.
  var copiers = [].slice.call(document.querySelectorAll("[data-copy]"));

  copiers.forEach(function (button) {
    var lines = [].slice.call(button.parentNode.querySelectorAll("code"));
    if (!lines.length || !navigator.clipboard) {
      button.hidden = true;
      return;
    }
    var label = button.textContent;
    var reset = 0;
    button.addEventListener("click", function () {
      var text = lines
        .map(function (line) {
          return line.textContent;
        })
        .join("\n");
      navigator.clipboard.writeText(text).then(function () {
        button.textContent = "Copied";
        button.setAttribute("data-copied", "");
        clearTimeout(reset);
        reset = setTimeout(function () {
          button.textContent = label;
          button.removeAttribute("data-copied");
        }, 1500);
      });
    });
  });

  // ---------------------------------------------------------------- walk
  // The frame shows the shot of the step nearest the middle of the window.
  // Every intersection change re-picks from scratch rather than following
  // which step just crossed a line, so a fast scroll cannot skip one and a
  // resize cannot leave the wrong one on. The frame is told which way the
  // reader went, so the incoming shot slides from that side.
  var walk = document.querySelector(".t-walk");
  var walkSteps = walk ? [].slice.call(walk.querySelectorAll(".t-step")) : [];
  var walkShots = walk ? [].slice.call(walk.querySelectorAll(".t-walk__shot")) : [];
  var walkCount = walk && walk.querySelector(".t-walk__count");

  if (walk && walkSteps.length === walkShots.length && "IntersectionObserver" in window) {
    var walkAt = 0;
    var walkRail = walk.querySelector(".t-walk__steps");
    var walkMarker = document.createElement("span");
    walkMarker.className = "t-walk__marker";
    walkMarker.setAttribute("aria-hidden", "true");
    walkRail.appendChild(walkMarker);

    // The diamond sits level with the middle of the first line of the step's
    // heading, measured from the top of the rail.
    function placeMarker() {
      var step = walkSteps[walkAt];
      var heading = step.querySelector("h3") || step;
      var line = parseFloat(getComputedStyle(heading).lineHeight) || heading.offsetHeight;
      // The rail is the positioned ancestor, so the heading's offset is
      // already from its top.
      var y = heading.offsetTop + Math.min(line, heading.offsetHeight) / 2;
      walkRail.style.setProperty("--marker-y", y + "px");
    }

    function showStep(index) {
      if (index === walkAt) return;
      walk.style.setProperty("--walk-direction", index > walkAt ? 1 : -1);
      walkAt = index;
      walkShots.forEach(function (shot, which) {
        shot.classList.toggle("is-on", which === index);
      });
      walkSteps.forEach(function (step, which) {
        step.classList.toggle("is-on", which === index);
      });
      if (walkCount) walkCount.textContent = channel(index + 1);
      placeMarker();
    }

    function pickStep() {
      var middle = innerHeight / 2;
      var best = 0;
      var bestDistance = Infinity;
      walkSteps.forEach(function (step, index) {
        var box = step.getBoundingClientRect();
        var distance = Math.abs(box.top + box.height / 2 - middle);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = index;
        }
      });
      showStep(best);
    }

    // Numbered as the channels of a set, to go with the monitor it is on.
    function channel(number) {
      function pad(value) {
        return value < 10 ? "0" + value : String(value);
      }
      return "CH " + pad(number) + " / " + pad(walkSteps.length);
    }

    if (walkCount) walkCount.textContent = channel(1);
    placeMarker();
    addEventListener("resize", placeMarker);

    // Where the frame stands beside the steps, it sticks in the middle of the
    // window below the header rather than against the header, so the shot
    // is level with the step being read. Stacked on a phone it stays under
    // the header, above the steps it scrolls with.
    var walkFrame = walk.querySelector(".t-walk__frame");
    var walkHeader = document.querySelector(".t-top");
    var sideBySide = matchMedia("(min-width: 60rem)");

    function centreFrame() {
      if (!sideBySide.matches || !walkFrame) {
        walk.style.removeProperty("--walk-top");
        return;
      }
      var header = walkHeader ? walkHeader.getBoundingClientRect().height : 0;
      var top = header + (innerHeight - header - walkFrame.offsetHeight) / 2;
      walk.style.setProperty("--walk-top", Math.max(header + 16, top) + "px");
    }

    centreFrame();
    addEventListener("resize", centreFrame);
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(placeMarker);
    var stepObserver = new IntersectionObserver(pickStep, {
      threshold: [0, 0.25, 0.5, 0.75, 1],
    });
    walkSteps.forEach(function (step) {
      stepObserver.observe(step);
    });
  }

  // -------------------------------------------------------------- viewer
  // The dialog is the browser's own, so Escape, the backdrop and the focus
  // trap are not ours to write. The set is walked by the two buttons in its
  // footer and by the arrow keys, and the shot names itself underneath: a
  // reader who has opened one wants to know what it is and to see the next.
  var viewer = document.getElementById("viewer");
  var openers = [].slice.call(document.querySelectorAll(".t-shot__open"));

  if (viewer && openers.length && typeof viewer.showModal === "function") {
    var shot = viewer.querySelector(".t-viewer__shot");
    var caption = viewer.querySelector(".t-viewer__caption");
    var count = viewer.querySelector(".t-viewer__count");
    var steps = [].slice.call(viewer.querySelectorAll("[data-step]"));
    var at = 0;

    function show(index) {
      at = (index + openers.length) % openers.length;
      var opener = openers[at];
      var picture = opener.querySelector("img");
      shot.src = opener.href;
      shot.alt = picture ? picture.alt : "";
      // The caption the figure already carries, or failing that the shot's
      // own description, rather than a second copy of either written for the
      // viewer and left to drift.
      var figure = opener.closest("figure");
      var text = figure && figure.querySelector("figcaption");
      caption.textContent = text ? text.textContent.trim() : shot.alt;
      if (count) count.textContent = at + 1 + " / " + openers.length;
    }

    steps.forEach(function (step) {
      step.addEventListener("click", function () {
        show(at + Number(step.dataset.step));
      });
    });

    openers.forEach(function (opener, index) {
      opener.addEventListener("click", function (event) {
        // A modified click is the reader asking for the file itself.
        if (event.metaKey || event.ctrlKey || event.shiftKey || event.button !== 0) return;
        event.preventDefault();
        show(index);
        viewer.showModal();
      });
    });

    viewer.addEventListener("click", function (event) {
      // The backdrop is the dialog itself: a click that lands on no child of
      // it closes, which is what clicking beside a picture means.
      if (event.target === viewer || event.target.hasAttribute("data-close")) viewer.close();
    });

    // On the document rather than on the dialog: a modal moves focus to the
    // first thing inside it, but a click on the backdrop or on the shot
    // leaves focus somewhere a listener bound to the dialog never hears from.
    // `viewer.open` is what scopes it, so the page's own arrow keys are
    // untouched while the viewer is closed.
    addEventListener("keydown", function (event) {
      if (!viewer.open) return;
      if (event.key === "ArrowRight") show(at + 1);
      else if (event.key === "ArrowLeft") show(at - 1);
      else return;
      event.preventDefault();
    });
  }

  // ---------------------------------------------------------------- boot
  // The console prints its checklist a line at a time, as a machine just
  // switched on would. With motion reduced it is simply there.
  if (bootConsole && !reducedMotion.matches) {
    var bootLines = [].slice.call(bootConsole.querySelectorAll("li"));
    var BOOT_START = 300;
    var BOOT_STEP = 150;
    bootConsole.classList.add("is-booting");
    bootLines.forEach(function (line, index) {
      setTimeout(
        function () {
          line.classList.add("is-up");
        },
        BOOT_START + index * BOOT_STEP,
      );
    });
    setTimeout(
      function () {
        bootConsole.classList.remove("is-booting");
      },
      BOOT_START + bootLines.length * BOOT_STEP + 120,
    );
  }

  // ---------------------------------------------------------------- keys
  // Omaweb reaches anything on a page with link hints, and the site answers
  // to the same key: F labels every link and button in view, and typing a
  // label follows it. T moves to the next theme. Neither fires while the
  // reader is typing into something, holding a modifier, or looking at a
  // screenshot in the viewer.
  var HINT_ALPHABET = "sadfjklewcmpgh";
  var hints = null;
  var keysLine = document.querySelector(".t-keys");
  if (keysLine) keysLine.hidden = false;

  function hintLabels(count) {
    var letters = HINT_ALPHABET.split("");
    if (count <= letters.length) return letters.slice(0, count);
    var labels = [];
    for (var first = 0; first < letters.length; first += 1) {
      for (var second = 0; second < letters.length; second += 1) {
        labels.push(letters[first] + letters[second]);
      }
    }
    return labels.slice(0, count);
  }

  // What the reader could click right now: in the window, and the topmost
  // thing at its own centre, which rules out a link under the header, a
  // shot the frame is not showing, and a menu that is folded away.
  function hintTargets() {
    return [].slice.call(document.querySelectorAll("a[href], button")).filter(function (element) {
      var box = element.getBoundingClientRect();
      if (!box.width || !box.height) return false;
      if (box.bottom < 0 || box.top > innerHeight || box.right < 0 || box.left > innerWidth) {
        return false;
      }
      var x = Math.min(Math.max(box.left + box.width / 2, 0), innerWidth - 1);
      var y = Math.min(Math.max(box.top + box.height / 2, 0), innerHeight - 1);
      var hit = document.elementFromPoint(x, y);
      return Boolean(hit) && (hit === element || element.contains(hit));
    });
  }

  function openHints() {
    var targets = hintTargets();
    if (!targets.length) return;
    var labels = hintLabels(targets.length);
    var layer = document.createElement("div");
    layer.className = "t-hints";
    layer.setAttribute("aria-hidden", "true");
    var items = targets.map(function (element, index) {
      var box = element.getBoundingClientRect();
      var node = document.createElement("span");
      node.className = "t-hint";
      node.textContent = labels[index];
      node.style.left = Math.max(12, box.left) + "px";
      node.style.top = Math.max(10, box.top) + "px";
      layer.appendChild(node);
      return { label: labels[index], element: element, node: node };
    });
    var bar = document.createElement("div");
    bar.className = "t-hints__bar";
    bar.textContent = "Type a label to follow it · Esc to cancel";
    layer.appendChild(bar);
    document.body.appendChild(layer);
    hints = { layer: layer, items: items, typed: "" };
  }

  function closeHints() {
    if (!hints) return;
    hints.layer.remove();
    hints = null;
  }

  function filterHints() {
    var typed = hints.typed;
    var left = hints.items.filter(function (item) {
      return item.label.indexOf(typed) === 0;
    });
    if (!left.length) {
      closeHints();
      return;
    }
    if (left.length === 1 && left[0].label === typed) {
      var element = left[0].element;
      closeHints();
      element.focus();
      element.click();
      return;
    }
    hints.items.forEach(function (item) {
      var match = item.label.indexOf(typed) === 0;
      item.node.hidden = !match;
      if (!match) return;
      item.node.textContent = "";
      var done = document.createElement("b");
      done.textContent = typed;
      item.node.appendChild(done);
      item.node.appendChild(document.createTextNode(item.label.slice(typed.length)));
    });
  }

  function nextTheme() {
    var current = document.body.dataset.theme || themeNames[0];
    var next = themeNames[(themeNames.indexOf(current) + 1) % themeNames.length];
    // Swept out of the middle of the window, where the reader is looking,
    // since the key has no button to sweep from.
    var middle = {
      getBoundingClientRect: function () {
        return { left: innerWidth / 2, top: innerHeight / 2, width: 0, height: 0 };
      },
    };
    setTheme(next, { save: true, share: true, from: middle });
  }

  addEventListener("keydown", function (event) {
    if (event.defaultPrevented || event.metaKey || event.ctrlKey || event.altKey) return;
    if (hints) {
      if (event.key === "Escape") closeHints();
      else if (event.key === "Backspace") {
        hints.typed = hints.typed.slice(0, -1);
        filterHints();
      } else if (/^[a-z]$/i.test(event.key)) {
        hints.typed += event.key.toLowerCase();
        filterHints();
      } else return;
      event.preventDefault();
      return;
    }
    var target = event.target;
    if (target.closest && target.closest("input, textarea, select, [contenteditable]")) return;
    if (document.querySelector("dialog[open]")) return;
    var key = event.key.toLowerCase();
    if (key === "f") openHints();
    else if (key === "t") nextTheme();
    else return;
    event.preventDefault();
  });
  // The labels are pinned to where things were, so anything that moves the
  // page takes them down.
  addEventListener("scroll", closeHints, { passive: true });
  addEventListener("resize", closeHints);
  addEventListener("pointerdown", closeHints);
})();
