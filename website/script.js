// Omaweb website behaviour: theme switching, and nothing else. It is
// optional, and the page renders and reads correctly with this file blocked
// -- index.html names the theme every themed asset starts on.
//
// One palette drives the page, the generated screenshots, the wordmark and
// the favicon, so picking a theme restyles all of them at once. The choice is
// saved locally, and a `theme` query parameter lets a shared URL choose its
// palette.

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

    themed.forEach(function (image) {
      image.src = image.dataset.themed.replace("{theme}", name);
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
})();
