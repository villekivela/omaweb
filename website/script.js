// Omaweb website behaviour. Two things, both optional: the page renders and
// reads correctly with this file blocked -- index.html names the theme every
// themed asset starts on, and each thumbnail is a plain link to its full
// screenshot.
//
//   1. Theme switching. One palette drives the page, the generated
//      screenshots, the wordmark and the favicon, so picking a theme restyles
//      all of them at once. The choice is saved locally, and a `theme` query
//      parameter lets a shared URL choose its palette.
//   2. Opening a screenshot in a viewer instead of navigating to the file.
//      A whole window drawn at grid width is unreadable whatever its
//      resolution, so the grid is thumbnails and this is how they are read.

(function () {
  "use strict";

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
