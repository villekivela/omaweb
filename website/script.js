// Omaweb website behaviour. Three things, all optional: the page renders and
// reads correctly with this file blocked -- index.html names the theme every
// themed asset starts on, each thumbnail is a plain link to its full
// screenshot, and the canvas behind the hero is decoration.
//
//   1. Theme switching. One palette drives the page, the generated
//      screenshots, the wordmark and the favicon, so picking a theme restyles
//      all of them at once. The choice is saved locally, and a `theme` query
//      parameter lets a shared URL choose its palette.
//   2. Opening a screenshot in a viewer instead of navigating to the file.
//      A whole window drawn at grid width is unreadable whatever its
//      resolution, so the grid is thumbnails and this is how they are read.
//   3. The rain behind the hero: the drawing the screenshots' wallpaper is
//      made of, in the active palette, falling slowly.

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

  function parseColor(value) {
    var digits = value.trim().replace("#", "");
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

  // The screenshots, the wordmark and the favicon are generated one per
  // theme, under paths that differ only in the theme's name. Each element
  // carries its own path with `{theme}` where that name goes, so adding a
  // themed image is markup and nothing here.
  var themed = [].slice.call(document.querySelectorAll("[data-themed]"));
  var favicon = document.querySelector('link[rel="icon"]');
  var themeColor = document.querySelector('meta[name="theme-color"]');

  function hasTheme(name) {
    return themeButtons.some(function (button) {
      return button.dataset.theme === name;
    });
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

    document.body.dataset.theme = name;
    themeButtons.forEach(function (button) {
      button.setAttribute("aria-pressed", String(button.dataset.theme === name));
    });

    themed.forEach(function (element) {
      var path = element.dataset.themed.replace("{theme}", name);
      if (element.tagName === "A") element.href = path;
      else element.src = path;
    });
    if (favicon) favicon.href = "assets/icons/favicon-" + name + ".svg";
    // The browser chrome around the page follows the palette too, read off
    // the ground the theme just painted rather than listed a second time.
    if (themeColor) {
      themeColor.content = getComputedStyle(document.body).getPropertyValue("--bg").trim();
    }
    if (rainContext) startRain();

    if (options.save) saveTheme(name);
    if (options.share) {
      var url = new URL(location.href);
      url.searchParams.set("theme", name);
      history.replaceState(null, "", url);
    }
  }

  themeButtons.forEach(function (button) {
    button.addEventListener("click", function () {
      setTheme(button.dataset.theme, { save: true, share: true });
    });
  });

  var queryTheme = new URLSearchParams(location.search).get("theme");
  var savedTheme = readSavedTheme();
  var initialTheme = hasTheme(queryTheme) ? queryTheme : savedTheme;
  if (hasTheme(initialTheme)) setTheme(initialTheme, { save: Boolean(queryTheme), share: false });

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
      var thumbnail = opener.querySelector("img");
      shot.src = opener.href;
      shot.alt = thumbnail ? thumbnail.alt : "";
      // The caption the figure already carries, rather than a second copy of
      // it written for the viewer and left to drift from the first.
      var figure = opener.closest("figure");
      var text = figure && figure.querySelector("figcaption");
      caption.textContent = text ? text.textContent.trim() : "";
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
