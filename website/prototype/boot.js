// PROTOTYPE (#440). Throwaway: never merged. The combined landing page on its
// own route, `?variant=drive` (`current` is the page as it is), a floating
// switcher at the bottom, a switch between the four night drive Scenes, and a
// drawer that shows what the page would mean for the browser's chrome. Mounted on
// localhost only, so a stray deploy of this file shows nobody anything.

(function () {
  "use strict";

  if (!/^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname)) return;

  var FILES = [
    "prototype/content.js",
    "prototype/scenes/display.js",
    "prototype/scenes/host.js",
    "prototype/scenes/road.js",
    "prototype/scenes/vector.js",
    "prototype/scenes/led.js",
    "prototype/scenes/dither.js",
    "prototype/scenes/pixel.js",
    "prototype/scenes/crt.js",
    "prototype/scenes/desert.js",
    "prototype/chrome.js",
    "prototype/variants/drive.js",
  ];

  var PV = (window.PV = { variants: {}, order: ["current", "drive"] });
  var params = new URLSearchParams(location.search);

  function stored(key, fallback) {
    try {
      return params.get(key) || localStorage.getItem("pv-" + key) || fallback;
    } catch (e) {
      return params.get(key) || fallback;
    }
  }
  function remember(key, value) {
    try {
      localStorage.setItem("pv-" + key, value);
    } catch (e) {}
    params.set(key, value);
    history.replaceState(null, "", "?" + params.toString() + location.hash);
  }

  PV.variant = params.get("variant") || "current";
  PV.theme = stored("theme", "omaweb");
  PV.scene = stored("scene", "crt-road");

  PV.el = function (tag, className, html) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (html !== undefined) node.innerHTML = html;
    return node;
  };

  // The Omaweb mark alone, the prompt the browser's own field draws (#439).
  PV.mark = function (className) {
    var source = document.querySelector(".brand__wordmark .wordmark__mark");
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", "20.2 97.1 31.3 19.2");
    svg.setAttribute("aria-hidden", "true");
    svg.setAttribute("class", "pv-mark " + (className || ""));
    var path = source.cloneNode(true);
    path.removeAttribute("class");
    svg.appendChild(path);
    return svg;
  };
  PV.wordmark = function (className) {
    var svg = document.querySelector(".brand__wordmark").cloneNode(true);
    svg.setAttribute("class", "wordmark " + (className || ""));
    return svg;
  };

  PV.shotTheme = function () {
    return PV.theme === "omaweb" ? "matte-black" : PV.theme;
  };
  PV.shot = function (name) {
    return "assets/shots/" + PV.shotTheme() + "/" + name + ".webp";
  };

  // Every element that themes: the variant's root and the chrome drawer.
  PV.setTheme = function (name) {
    PV.theme = name;
    remember("theme", name);
    document.querySelectorAll("[data-pv-themed]").forEach(function (node) {
      node.dataset.theme = name;
    });
    document.querySelectorAll("img[data-shot]").forEach(function (img) {
      img.src = PV.shot(img.dataset.shot);
    });
    document.querySelectorAll("[data-pv-theme-choice]").forEach(function (button) {
      button.setAttribute("aria-pressed", String(button.dataset.pvThemeChoice === name));
    });
    document.querySelectorAll("[data-pv-theme-name]").forEach(function (node) {
      node.textContent = PV.themeName();
    });
    if (window.OmawebScenes) OmawebScenes.retheme();
    document.dispatchEvent(new CustomEvent("pv-theme"));
  };
  PV.themeName = function () {
    var names = PV_CONTENT.themes.filter(function (t) {
      return t[0] === PV.theme;
    });
    return names.length ? names[0][1] : PV.theme;
  };
  PV.stepTheme = function (by) {
    var list = PV_CONTENT.themes.map(function (t) {
      return t[0];
    });
    var at = list.indexOf(PV.theme);
    PV.setTheme(list[(at + by + list.length) % list.length]);
  };

  // A row of theme swatches, each in that theme's own ground and accent.
  PV.swatches = function (className) {
    var row = PV.el("div", "pv-swatches " + (className || ""));
    row.setAttribute("role", "group");
    row.setAttribute("aria-label", "Theme");
    PV_CONTENT.themes.forEach(function (t) {
      var b = PV.el("button", "pv-swatch");
      b.type = "button";
      b.dataset.theme = t[0];
      b.dataset.pvThemeChoice = t[0];
      b.title = t[1];
      b.setAttribute("aria-label", t[1]);
      b.setAttribute("aria-pressed", String(t[0] === PV.theme));
      b.addEventListener("click", function () {
        PV.setTheme(t[0]);
      });
      row.appendChild(b);
    });
    return row;
  };

  // The reader's choices for Scene options, by option name.
  PV.chosen = {};

  PV.hostScene = function (canvas, options) {
    options = options || {};
    options.scene = PV.scene;
    options.chosen = PV.chosen;
    return new OmawebScenes.Host(canvas, options);
  };

  function sceneOptions() {
    var list = OmawebScenes.list().filter(function (scene) {
      return scene.id === PV.scene;
    });
    return (list[0] && list[0].options) || {};
  }

  // One bar button per option the Scene declares, cycling its values.
  function optionButtons() {
    var holder = document.querySelector(".pvbar__options");
    if (!holder) return;
    holder.textContent = "";
    var declared = sceneOptions();
    Object.keys(declared).forEach(function (name) {
      var values = declared[name];
      var button = PV.el("button", "pvbar__toggle");
      button.type = "button";
      function label() {
        var value = values.indexOf(PV.chosen[name]) >= 0 ? PV.chosen[name] : values[0];
        button.textContent = name.charAt(0).toUpperCase() + name.slice(1) + ": " + value;
        return value;
      }
      label();
      button.addEventListener("click", function () {
        var at = values.indexOf(label());
        PV.chosen[name] = values[(at + 1) % values.length];
        remember("opt-" + name, PV.chosen[name]);
        OmawebScenes.hosts.forEach(function (host) {
          host.setOption(name, PV.chosen[name]);
        });
        label();
      });
      holder.appendChild(button);
    });
  }
  PV.setScene = function (id) {
    PV.scene = id;
    remember("scene", id);
    OmawebScenes.use(id);
    document.querySelectorAll("[data-pv-themed]").forEach(function (node) {
      node.dataset.scene = id;
    });
    var label = document.querySelector(".pvbar__scene");
    if (label) label.textContent = sceneName();
    optionButtons();
  };
  function sceneName() {
    var list = OmawebScenes.list();
    for (var i = 0; i < list.length; i++) if (list[i].id === PV.scene) return list[i].name;
    return PV.scene;
  }

  function load(index, done) {
    if (index === FILES.length) return done();
    var script = document.createElement("script");
    script.src = FILES[index];
    script.onload = function () {
      load(index + 1, done);
    };
    document.body.appendChild(script);
  }

  function mount() {
    OmawebScenes.list().forEach(function (scene) {
      Object.keys(scene.options || {}).forEach(function (name) {
        var value = stored("opt-" + name, "");
        if (value) PV.chosen[name] = value;
      });
    });
    // A Scene remembered from an earlier build of this prototype may be gone.
    if (
      !OmawebScenes.list().some(function (scene) {
        return scene.id === PV.scene;
      })
    ) {
      PV.scene = OmawebScenes.list()[0].id;
    }
    var variant = PV.variants[PV.variant];
    document.documentElement.dataset.pvVariant = PV.variant;
    if (variant) {
      var root = PV.el("div", "pv pv-" + PV.variant);
      root.dataset.pvThemed = "";
      root.dataset.theme = PV.theme;
      root.dataset.scene = PV.scene;
      document.body.insertBefore(root, document.body.firstChild);
      variant.mount(root);
    }
    bar();
    PV.setTheme(PV.theme);
    if (params.get("perf") && OmawebScenes.hosts.length) measure();
  }

  function bar() {
    var keys = PV.order;
    var at = keys.indexOf(PV.variant);
    if (at < 0) at = 0;
    function go(by) {
      var next = keys[(at + by + keys.length) % keys.length];
      params.set("variant", next);
      location.search = params.toString();
    }
    var name = PV.variant === "current" ? "The page as it is" : PV.variants[PV.variant].name;
    var node = PV.el(
      "div",
      "pvbar",
      '<button type="button" class="pvbar__step" data-by="-1" aria-label="Previous variant">←</button>' +
        '<span class="pvbar__name"><b>' +
        PV.variant.toUpperCase() +
        "</b> " +
        name +
        "</span>" +
        '<button type="button" class="pvbar__step" data-by="1" aria-label="Next variant">→</button>' +
        '<span class="pvbar__sep"></span>' +
        '<button type="button" class="pvbar__toggle" data-scene>Scene: <span class="pvbar__scene"></span></button>' +
        '<span class="pvbar__options"></span>' +
        '<button type="button" class="pvbar__toggle" data-chrome aria-pressed="false">Chrome</button>',
    );
    node.querySelector(".pvbar__scene").textContent = sceneName();
    node.querySelectorAll("[data-by]").forEach(function (b) {
      b.addEventListener("click", function () {
        go(+b.dataset.by);
      });
    });
    node.querySelector("[data-scene]").addEventListener("click", function () {
      var list = OmawebScenes.list();
      var i = list.findIndex(function (s) {
        return s.id === PV.scene;
      });
      PV.setScene(list[(i + 1) % list.length].id);
    });
    var chromeButton = node.querySelector("[data-chrome]");
    chromeButton.addEventListener("click", function () {
      var open = chromeButton.getAttribute("aria-pressed") !== "true";
      chromeButton.setAttribute("aria-pressed", String(open));
      PV.chromeDrawer(open);
    });
    if (PV.variant === "current") chromeButton.hidden = true;
    document.body.appendChild(node);
    optionButtons();
    document.addEventListener("keydown", function (event) {
      var t = event.target;
      if (t.closest && t.closest("input, textarea, [contenteditable]")) return;
      if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
      if (event.key === "ArrowLeft") go(-1);
      if (event.key === "ArrowRight") go(1);
    });
  }

  PV.chromeDrawer = function (open) {
    var drawer = document.querySelector(".pv-drawer");
    if (!drawer) {
      var variant = PV.variants[PV.variant];
      drawer = PV.el("aside", "pv-drawer");
      drawer.dataset.pvThemed = "";
      drawer.dataset.theme = PV.theme;
      drawer.setAttribute("aria-label", "What this means for the browser");
      drawer.appendChild(
        PV.el("p", "pv-drawer__title", "In the browser <small>" + variant.name + "</small>"),
      );
      drawer.appendChild(PV.chrome(variant.chrome.lang));
      var notes = PV.el("ul", "pv-drawer__notes");
      variant.chrome.notes.forEach(function (n) {
        notes.appendChild(PV.el("li", "", n));
      });
      drawer.appendChild(notes);
      document.body.appendChild(drawer);
    }
    drawer.classList.toggle("is-open", open);
  };

  // PROTOTYPE measurement: `?perf=1` measures the Scene for 8 s at rest on the
  // Start page, then 8 s while scrolling, and reports each to the local
  // server as a GET it logs: frames drawn, main-thread milliseconds per draw,
  // and the interval between drawn frames.
  function measure() {
    // The same work in every browser: a window not in front still draws, and
    // the Scene fills one 1440 by 900 box whatever the window's size.
    // `perf=idle` measures the page with its Scene not drawing: the floor.
    OmawebScenes.ignoreFocus = params.get("perf") !== "idle";
    document.documentElement.dataset.perf = "";
    var host = OmawebScenes.hosts[0];
    host.layout();
    host.update();
    var gaps = [];
    var last = 0;
    var watching = false;
    var draw = host.draw;
    host.draw = function () {
      draw.call(host);
      var now = performance.now();
      if (watching && last) gaps.push(now - last);
      last = now;
    };
    function phase(name, seconds, then) {
      fetch("/__perf?start=" + name).catch(function () {});
      OmawebScenes.parts = { pic: 0, glass: 0 };
      host.resetStats();
      gaps = [];
      last = 0;
      watching = true;
      setTimeout(function () {
        watching = false;
        var s = host.stats;
        var secs = (performance.now() - s.since) / 1000;
        gaps.sort(function (a, b) {
          return a - b;
        });
        var at = function (q) {
          return gaps.length
            ? gaps[Math.min(gaps.length - 1, Math.floor(gaps.length * q))].toFixed(1)
            : "-";
        };
        var report = [
          "phase=" + name,
          "ua=" +
            encodeURIComponent(
              navigator.userAgent.replace(
                /.*(Firefox\/[\d.]+|Chrome\/[\d.]+|Version\/[\d.]+ Safari).*/,
                "$1",
              ),
            ),
          "viewport=" + innerWidth + "x" + innerHeight + "@" + devicePixelRatio,
          "scene=" + PV.scene,
          "fps=" + (s.draws / secs).toFixed(1),
          "drawMs=" + (s.drawMs / Math.max(1, s.draws)).toFixed(2),
          "maxMs=" + s.maxMs.toFixed(1),
          "gapP50=" + at(0.5),
          "gapP95=" + at(0.95),
          "focus=" + document.hasFocus(),
          "picMs=" + (((OmawebScenes.parts || {}).pic || 0) / Math.max(1, s.draws)).toFixed(2),
          "glassMs=" + (((OmawebScenes.parts || {}).glass || 0) / Math.max(1, s.draws)).toFixed(2),
        ].join("&");
        fetch("/__perf?" + report).catch(function () {});
        then();
      }, seconds * 1000);
    }
    window.scrollTo(0, 0);
    setTimeout(function () {
      phase("hero", 8, function () {
        var dir = 1;
        var scroller = setInterval(function () {
          if (window.scrollY > 2000) dir = -1;
          if (window.scrollY < 200) dir = 1;
          window.scrollBy(0, 14 * dir);
        }, 16);
        setTimeout(function () {
          phase("scroll", 8, function () {
            clearInterval(scroller);
            window.scrollTo(0, 0);
          });
        }, 1000);
      });
    }, 4000);
  }

  var css = document.createElement("link");
  css.rel = "stylesheet";
  css.href = "prototype/prototype.css";
  document.head.appendChild(css);
  load(0, mount);
})();
