// Omaweb website behaviour. Three things, all optional: the page renders and
// reads correctly with this file blocked.
//
//   1. Theme switching. One palette drives the page and the product UI, so
//      picking a theme restyles both at once, which is what
//      `omarchy theme set` does to the real browser.
//   2. Hiding the product UI's sidebar, on Ctrl/Cmd+B and on either panel
//      icon, the way the application binds it.
//   3. Parallax between the desktop wallpaper and the window on it.

(function () {
  "use strict";

  var reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;

  // ---------------------------------------------------------------- theme
  var themeButtons = [].slice.call(document.querySelectorAll(".t-theme"));

  function setTheme(name) {
    document.body.dataset.theme = name;
    themeButtons.forEach(function (button) {
      button.setAttribute("aria-pressed", String(button.dataset.theme === name));
    });
  }

  themeButtons.forEach(function (button) {
    button.addEventListener("click", function () {
      setTheme(button.dataset.theme);
    });
  });

  // ------------------------------------------------------------- sidebar
  // The icon swaps the same way the application does: left_panel_open once
  // the sidebar is hidden. While it is hidden the navigation cluster floats
  // over the page, and its own icon is how the sidebar comes back.
  var ui = document.getElementById("ui");
  var panelButtons = [].slice.call(document.querySelectorAll(".ui__panel"));

  function setCollapsed(collapsed) {
    ui.dataset.collapsed = String(collapsed);
    panelButtons.forEach(function (button) {
      var glyph = button.querySelector(".ms");
      var label = button.querySelector(".sr-only");
      button.setAttribute("aria-pressed", String(collapsed));
      if (glyph) {
        glyph.classList.toggle("ms--left-panel-open", collapsed);
        glyph.classList.toggle("ms--left-panel-close", !collapsed);
      }
      if (label) label.textContent = collapsed ? "Show sidebar" : "Hide sidebar";
    });
  }

  if (ui) {
    panelButtons.forEach(function (button) {
      button.addEventListener("click", function () {
        setCollapsed(ui.dataset.collapsed !== "true");
      });
    });

    addEventListener("keydown", function (event) {
      if (!event.metaKey && !event.ctrlKey) return;
      if (event.key.toLowerCase() !== "b") return;
      event.preventDefault();
      setCollapsed(ui.dataset.collapsed !== "true");
    });
  }

  // ------------------------------------------------------------ parallax
  // Two layers at different rates. One is not enough: a repeating diagonal
  // stripe field shifted along its own axis is indistinguishable from
  // itself, so the orb layer is the landmark you can actually track and the
  // stripes travel at under half its rate.
  var desk = document.querySelector(".desk");
  var wall = document.querySelector(".desk__wall");
  var grain = document.querySelector(".desk__grain");

  if (desk && wall && grain && !reduced) {
    var RANGE = 210;
    var GRAIN_RATE = 0.42;
    var queued = false;

    function park() {
      queued = false;
      var box = desk.getBoundingClientRect();
      if (box.bottom < 0 || box.top > innerHeight) return;

      // +1 while the desktop is entering, -1 as it leaves. Clamped: the raw
      // ratio overshoots 1 at both ends, which would push a layer past its
      // headroom and expose a strip of bare background.
      var progress = 1 - 2 * ((box.top + box.height / 2) / (innerHeight + box.height));
      progress = Math.max(-1, Math.min(1, progress));

      var shift = progress * RANGE;
      wall.style.setProperty("--par", shift.toFixed(1) + "px");
      grain.style.setProperty("--grain", (shift * GRAIN_RATE).toFixed(1) + "px");
    }

    function onScroll() {
      if (queued) return;
      queued = true;
      requestAnimationFrame(park);
    }

    addEventListener("scroll", onScroll, { passive: true });
    addEventListener("resize", onScroll);
    park();
  }
})();
