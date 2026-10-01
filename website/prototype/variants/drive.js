// PROTOTYPE (#440). The combined page: A's Start page hero, then C's drive.
// One Scene stands behind the whole page. The first screen is the Start page
// with the Omnibar on the horizon, its rows the walk; scrolling past it is
// the drive, the road speeding with the scroll, and the rest of the page
// passes as signs set close together. On a phone or with reduced motion the
// road holds a still frame and the signs read as an ordinary list.

(function () {
  "use strict";
  var C = window.PV_CONTENT;

  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;");
  }

  function kbd(key) {
    return key ? "<kbd>" + esc(key) + "</kbd>" : "";
  }

  PV.variants.drive = {
    name: "Start page, then the drive",
    chrome: {
      lang: "c",
      notes: [
        "The Start page is already this page's first screen: the Omnibar resting on the road, its rows the reader's own. The website takes its look from it, so the chrome keeps its own.",
        "One change worth its own ticket: on the Start page the road runs under the whole window, and the sidebar stands over it at the theme's own opacity instead of its flat ground. Once a page loads, the sidebar returns to its ground.",
        "The Scene is the shared part. The site and the Start page run the same contract, so a reader's own Scene would show on both, and the road speeds up as the reader navigates: a commit in the browser, a scroll on the site.",
      ],
    },
    mount: function (root) {
      var steps = C.steps;
      root.innerHTML =
        '<div class="dr-scene" aria-hidden="true"><canvas></canvas></div>' +
        '<div class="dr-scrim" aria-hidden="true"></div>' +
        '<header class="dr-top"><a class="dr-brand" href="#" aria-label="Omaweb"></a><nav aria-label="Main">' +
        '<a href="#dr-walk">Walk</a><a href="#dr-keys">Keyboard</a><a href="https://github.com/villekivela/omaweb#readme">Docs</a>' +
        '<a href="releases/">Releases</a><a href="https://github.com/villekivela/omaweb">GitHub</a></nav></header>' +
        // The Start page.
        '<section class="dr-start">' +
        '<div class="dr-sky"><p class="dr-kicker">' +
        esc(C.hero.kicker) +
        '</p><h1 class="dr-title">' +
        C.hero.title.map(esc).join("<br>") +
        "</h1></div>" +
        '<div class="dr-omnibar" role="search">' +
        '<label class="dr-field"><span class="dr-prompt"></span>' +
        '<input type="text" role="combobox" aria-expanded="true" aria-controls="dr-list" aria-label="Find on this page" placeholder="Where to?" autocomplete="off" spellcheck="false">' +
        "<kbd>o</kbd></label>" +
        '<ul class="dr-list" id="dr-list" role="listbox">' +
        steps
          .map(function (s, i) {
            return (
              '<li class="dr-row" role="option" id="dr-opt-' +
              s.id +
              '" data-step="' +
              s.id +
              '" aria-selected="' +
              (i === 0) +
              '"><span class="dr-row__label">' +
              esc(s.label) +
              '</span><span class="dr-row__title">' +
              esc(s.title) +
              "</span>" +
              kbd(s.key) +
              "</li>"
            );
          })
          .join("") +
        "</ul>" +
        '<div class="dr-hints"><span>↑↓ select</span><span>⏎ go</span><span class="dr-theme">Theme <kbd>T</kbd> <b data-pv-theme-name></b></span></div>' +
        "</div>" +
        '<div class="dr-below"><a class="dr-install" href="#dr-install">' +
        esc(C.hero.cta) +
        '</a><p class="dr-lede">' +
        C.hero.lede.map(esc).join(" ") +
        "</p></div>" +
        "</section>" +
        // The drive: the walk as signs, one pinned capture beside them.
        '<section class="dr-walk" id="dr-walk" aria-label="The walk">' +
        '<ol class="dr-signs">' +
        steps
          .map(function (s, i) {
            return (
              '<li class="dr-sign" id="dr-' +
              s.id +
              '" data-step="' +
              s.id +
              '" tabindex="0"' +
              (i === 0 ? ' aria-current="step"' : "") +
              '><p class="dr-sign__row"><span class="dr-exit">Exit ' +
              (i + 1) +
              '</span><span class="dr-sign__label">' +
              esc(s.label) +
              "</span>" +
              kbd(s.key) +
              "</p><h2>" +
              esc(s.title) +
              "</h2><p>" +
              esc(s.body) +
              '</p><img class="dr-sign__shot" data-shot="' +
              s.shot +
              '" alt="' +
              esc(s.alt) +
              '" width="2720" height="1720" loading="lazy" decoding="async"></li>'
            );
          })
          .join("") +
        "</ol>" +
        '<figure class="dr-mirror"><img data-shot="' +
        steps[0].shot +
        '" alt="' +
        esc(steps[0].alt) +
        '" width="2720" height="1720" decoding="async"><figcaption></figcaption></figure>' +
        "</section>" +
        // A gantry: what it does, overhead.
        '<section class="dr-gantry" aria-label="Features"><ul>' +
        C.features
          .map(function (f) {
            return '<li class="dr-plate"><h3>' + esc(f[0]) + "</h3><p>" + esc(f[1]) + "</p></li>";
          })
          .join("") +
        "</ul></section>" +
        '<section class="dr-pair">' +
        '<div class="dr-plate dr-keys" id="dr-keys"><h2>Learn once. Move faster forever.</h2>' +
        "<p>Keys borrowed from Vim, every one yours to rebind in <code>keybindings.json</code>.</p><dl>" +
        C.keys
          .map(function (k) {
            return "<div><dt>" + kbd(k[0]) + "</dt><dd>" + esc(k[1]) + "</dd></div>";
          })
          .join("") +
        "</dl></div>" +
        '<div class="dr-plate dr-install-sign" id="dr-install"><h2>' +
        C.install.title.map(esc).join(" ") +
        "</h2><p>" +
        esc(C.install.body) +
        '</p><p class="dr-step">1 Add the repository to <code>/etc/pacman.conf</code></p><pre>' +
        esc(C.install.repo) +
        '</pre><p class="dr-step">2 Trust the key, then install</p><pre>' +
        C.install.commands
          .map(function (c) {
            return '<span class="dr-dollar">$ </span>' + esc(c);
          })
          .join("\n") +
        "</pre><p>" +
        esc(C.install.after) +
        "</p></div></section>" +
        '<footer class="dr-foot"><span>Web navigation system. Free and open source under MPL 2.0.</span>' +
        '<a href="releases/">Releases</a><a href="https://github.com/villekivela/omaweb">GitHub</a></footer>';

      root.querySelector(".dr-brand").appendChild(PV.wordmark());
      root.querySelector(".dr-prompt").appendChild(PV.mark());
      root.querySelector(".dr-hints").appendChild(PV.swatches("dr-swatches"));
      var host = PV.hostScene(root.querySelector(".dr-scene canvas"));

      // The walk: the sign in view, or the one pointed at or focused, puts
      // its capture in the mirror.
      var signs = [].slice.call(root.querySelectorAll(".dr-sign"));
      var mirror = root.querySelector(".dr-mirror img");
      var caption = root.querySelector(".dr-mirror figcaption");
      function show(sign) {
        var step = steps.filter(function (s) {
          return s.id === sign.dataset.step;
        })[0];
        signs.forEach(function (s) {
          if (s === sign) s.setAttribute("aria-current", "step");
          else s.removeAttribute("aria-current");
        });
        mirror.dataset.shot = step.shot;
        mirror.src = PV.shot(step.shot);
        mirror.alt = step.alt;
        caption.textContent = step.label;
      }
      show(signs[0]);
      var seen = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (e) {
            if (e.isIntersecting) show(e.target);
          });
        },
        { rootMargin: "-48% 0px -48% 0px" },
      );
      signs.forEach(function (s) {
        seen.observe(s);
        s.addEventListener("mouseenter", function () {
          show(s);
        });
        s.addEventListener("focus", function () {
          show(s);
        });
      });

      // The Omnibar: type to filter the walk, arrows to pick, Return to go.
      var input = root.querySelector(".dr-field input");
      var options = [].slice.call(root.querySelectorAll(".dr-row"));
      var selected = 0;
      function shown() {
        return options.filter(function (o) {
          return !o.hidden;
        });
      }
      function select(index) {
        var list = shown();
        if (!list.length) return;
        selected = (index + list.length) % list.length;
        options.forEach(function (o) {
          o.setAttribute("aria-selected", "false");
        });
        list[selected].setAttribute("aria-selected", "true");
        input.setAttribute("aria-activedescendant", list[selected].id);
      }
      var smooth = !matchMedia("(prefers-reduced-motion: reduce)").matches;
      function go(option) {
        var sign = root.querySelector("#dr-" + option.dataset.step);
        sign.scrollIntoView({ behavior: smooth ? "smooth" : "auto", block: "center" });
        show(sign);
        sign.focus({ preventScroll: true });
      }
      input.addEventListener("input", function () {
        var q = input.value.trim().toLowerCase();
        options.forEach(function (o) {
          o.hidden = !!q && o.textContent.toLowerCase().indexOf(q) < 0;
        });
        select(0);
      });
      input.addEventListener("keydown", function (e) {
        if (e.key === "ArrowDown" || e.key === "ArrowUp") {
          select(selected + (e.key === "ArrowDown" ? 1 : -1));
          e.preventDefault();
        } else if (e.key === "Enter") {
          var list = shown();
          if (list.length) go(list[selected]);
        } else if (e.key === "Escape") {
          input.value = "";
          input.dispatchEvent(new Event("input"));
          input.blur();
        }
      });
      options.forEach(function (o) {
        o.addEventListener("mouseenter", function () {
          select(shown().indexOf(o));
        });
        o.addEventListener("click", function () {
          go(o);
        });
      });

      // Past the Start page the road dims under a scrim of the theme's
      // ground, so the signs read against it; and the scroll's speed is how
      // hard the reader is navigating.
      var start = root.querySelector(".dr-start");
      var scrim = root.querySelector(".dr-scrim");
      var lastY = window.scrollY;
      var lastT = performance.now();
      var pace = 0;
      var easing = 0;
      function settle() {
        pace *= 0.9;
        host.setNavigating(pace);
        easing = pace > 0.01 ? requestAnimationFrame(settle) : 0;
      }
      addEventListener(
        "scroll",
        function () {
          var now = performance.now();
          var dy = Math.abs(window.scrollY - lastY);
          var dt = Math.max(16, now - lastT);
          lastY = window.scrollY;
          lastT = now;
          pace = Math.max(pace, Math.min(1, dy / dt / 3));
          host.setNavigating(pace);
          if (!easing) easing = requestAnimationFrame(settle);
          var past = Math.min(1, window.scrollY / (start.offsetHeight * 0.7));
          scrim.style.opacity = String(past);
        },
        { passive: true },
      );

      document.addEventListener("keydown", function (e) {
        if (e.target.closest("input, textarea") || e.ctrlKey || e.metaKey || e.altKey) return;
        if (e.key === "o") {
          e.preventDefault();
          window.scrollTo({ top: 0 });
          input.focus();
        } else if (e.key === "t" || e.key === "T") {
          PV.stepTheme(e.shiftKey ? -1 : 1);
        }
      });
    },
  };
})();
