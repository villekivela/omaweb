// Omaweb website behaviour. Five things, all optional: the page renders and
// reads correctly with this file blocked -- index.html names the theme every
// themed asset starts on, each shot is a plain link to its full screenshot,
// the first shot is the one on show, the canvas behind the hero is
// decoration, and the nav is in view until the script folds it behind its
// button.
//
//   1. Theme switching. One palette drives the page and the generated
//      screenshots, so picking a theme restyles both at once, and a pick
//      sweeps the new palette out of the button pressed. The choice is saved
//      locally, and a `theme` query parameter lets a shared URL choose its
//      palette. In Omaweb the reader's own theme is one of the choices, and
//      the one the page starts on.
//   2. The walk: which shot the pinned frame shows, which is the one beside
//      the step nearest the middle of the window.
//   3. Opening a screenshot in a viewer instead of navigating to the file:
//      a whole window drawn at column width is unreadable whatever its
//      resolution, and this is how it is read.
//   4. The rain behind the hero: the drawing the screenshots' wallpaper is
//      made of, in the active palette, falling slowly.
//   5. The menu a phone gets in place of the row of links in the header.

(function () {
  "use strict";

  // ----------------------------------------------------------------- rain
  // Square cells on a coarse grid, falling in columns from the top edge and
  // thinning out as they fall, in four tints of the accent. The same drawing
  // as `wallpaper()` in scripts/build_website_themes.py, and a change to one
  // wants the same change to the other; only the hash differs, since this one
  // has no 64-bit integers to mix. Time enters as a per-column row offset
  // into the hash, so the pattern slides down each column while the
  // brightness stays anchored to the top, which is what falling looks like.
  var rain = document.querySelector(".t-rain");
  var rainContext = rain && rain.getContext && rain.getContext("2d");
  var reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");

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

  function css(color) {
    return "rgb(" + color.map(Math.round).join(",") + ")";
  }

  function paintRain(time) {
    var style = getComputedStyle(document.body);
    var background = parseColor(style.getPropertyValue("--bg"));
    var foreground = parseColor(style.getPropertyValue("--fg"));
    var accent = parseColor(style.getPropertyValue("--accent"));
    var tints = [
      mix(background, accent, 0.22),
      mix(background, accent, 0.42),
      mix(background, accent, 0.68),
      accent,
      mix(accent, foreground, 0.55),
    ].map(css);

    var width = rain.clientWidth;
    var height = rain.clientHeight;
    var ratio = Math.min(devicePixelRatio || 1, 2);
    if (rain.width !== width * ratio || rain.height !== height * ratio) {
      rain.width = width * ratio;
      rain.height = height * ratio;
    }
    rainContext.setTransform(ratio, 0, 0, ratio, 0, 0);
    rainContext.clearRect(0, 0, width, height);

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
        rainContext.fillStyle = tints[level];
        rainContext.fillRect(column * pitch + inset, row * pitch + inset, cell, cell);
      }
    }
  }

  var rainFrame = 0;
  var rainPainted = -Infinity;

  function stepRain(time) {
    // Cells move whole pitches, so nothing between one slide and the next
    // needs drawing: a frame every 120ms keeps the canvas idle most of the
    // time and the motion no less continuous.
    if (time - rainPainted >= 120) {
      paintRain(time);
      rainPainted = time;
    }
    rainFrame = requestAnimationFrame(stepRain);
  }

  function startRain() {
    cancelAnimationFrame(rainFrame);
    if (reducedMotion.matches) paintRain(0);
    else rainFrame = requestAnimationFrame(stepRain);
  }

  if (rainContext) {
    startRain();
    addEventListener("resize", startRain);
    reducedMotion.addEventListener("change", startRain);
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
    if (rainContext) startRain();
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
      document.startViewTransition(apply);
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
      if (walkCount) walkCount.textContent = index + 1 + " / " + walkSteps.length;
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

    if (walkCount) walkCount.textContent = "1 / " + walkSteps.length;
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
})();
