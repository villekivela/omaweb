// PROTOTYPE (#440). Variant C, "The drive": one Scene fills the window behind
// the whole page and the reader's scroll is the drive. The page's clock is the
// scroll position rather than time, so the road moves only when the reader
// does, and the content stands beside the road as signs: the gantry over the
// first screen, then one exit per step of the walk.

(function () {
  "use strict";
  var C = window.PV_CONTENT;

  PV.variants.c = {
    name: "The drive",
    chrome: {
      lang: "c",
      notes: [
        "On the Start page the road runs under the whole window, not only the page area: the sidebar and the Omnibar stand over it at the theme's own opacity instead of the sidebar's flat ground.",
        "Everywhere else the chrome is as today. Once a page loads, the sidebar returns to its ground.",
        "In QML the StartPage moves from the page area to the window, under the sidebar, and the sidebar's fill on the Start page takes the opacity Omarchy's theme already names. The frame rule stays: the road draws nothing while hidden or unfocused.",
        "Driving the road from the reader's input is the website's alone. The app's road keeps time, as the contract says.",
      ],
    },
    mount: function (root) {
      root.innerHTML =
        '<div class="pc-scene" aria-hidden="true"><canvas></canvas></div>' +
        '<header class="pc-top"><a class="pc-brand" href="#"></a><span class="pc-theme"><span data-pv-theme-name></span></span></header>' +
        '<section class="pc-first">' +
        '<nav class="pc-gantry" aria-label="Main">' +
        '<a class="pc-sign" href="https://github.com/villekivela/omaweb"><span>GitHub</span><b>←</b></a>' +
        '<a class="pc-sign pc-sign--main" href="#pc-install"><span>Install</span><b>↓</b></a>' +
        '<a class="pc-sign" href="https://github.com/villekivela/omaweb#readme"><span>Docs</span><b>→</b></a>' +
        "</nav>" +
        '<div class="pc-plate pc-plate--hero"><p class="pc-kicker">' +
        C.hero.kicker +
        '</p><h1 class="pc-title">' +
        C.hero.title.join("<br>") +
        "</h1><p>" +
        C.hero.lede.join("<br>") +
        '</p><p class="pc-scroll">Scroll to drive</p></div>' +
        "</section>" +
        C.steps
          .map(function (s, i) {
            return (
              '<section class="pc-exit"><div class="pc-plate"><p class="pc-exit__tag">Exit ' +
              (i + 1) +
              '</p><p class="pc-exit__label">' +
              s.label +
              (s.key ? " <kbd>" + s.key + "</kbd>" : "") +
              "</p><h2>" +
              s.title +
              "</h2><p>" +
              s.body +
              '</p></div><figure class="pc-mirror"><img data-shot="' +
              s.shot +
              '" alt="' +
              s.alt +
              '" width="2720" height="1720" loading="lazy" decoding="async"></figure></section>'
            );
          })
          .join("") +
        '<section class="pc-exit pc-exit--keys"><div class="pc-plate"><p class="pc-exit__tag">Rest stop</p><h2>Learn once. Move faster forever.</h2>' +
        "<p>Keys borrowed from Vim, every one of them yours to rebind in <code>keybindings.json</code>.</p><dl>" +
        C.keys
          .map(function (k) {
            return "<div><dt><kbd>" + k[0] + "</kbd></dt><dd>" + k[1] + "</dd></div>";
          })
          .join("") +
        "</dl></div></section>" +
        '<section class="pc-exit pc-exit--install" id="pc-install"><div class="pc-plate pc-plate--wide"><p class="pc-exit__tag">Destination</p><h2 class="pc-title pc-title--md">' +
        C.install.title.join("<br>") +
        "</h2><p>" +
        C.install.body +
        '</p><p class="pc-step">1 Add the repository to /etc/pacman.conf</p><pre>' +
        C.install.repo +
        '</pre><p class="pc-step">2 Trust the key, then install</p><pre>' +
        C.install.commands
          .map(function (c) {
            return "$ " + c;
          })
          .join("\n") +
        "</pre><p>" +
        C.install.after +
        "</p></div></section>" +
        '<footer class="pc-foot"><span>Web navigation system. Free and open source under MPL 2.0.</span><a href="releases/">Releases</a><a href="https://github.com/villekivela/omaweb">GitHub</a></footer>';

      root.querySelector(".pc-brand").appendChild(PV.wordmark());
      root.querySelector(".pc-theme").appendChild(PV.swatches("pc-swatches"));

      var host = PV.hostScene(root.querySelector(".pc-scene canvas"), { clock: "manual" });
      var queued = false;
      function drive() {
        queued = false;
        host.setTime(window.scrollY / 260);
      }
      addEventListener(
        "scroll",
        function () {
          if (!queued) {
            queued = true;
            requestAnimationFrame(drive);
          }
        },
        { passive: true },
      );
      document.addEventListener("keydown", function (e) {
        if (e.target.closest("input, textarea") || e.ctrlKey || e.metaKey || e.altKey) return;
        if (e.key === "t" || e.key === "T") PV.stepTheme(e.shiftKey ? -1 : 1);
      });
    },
  };
})();
