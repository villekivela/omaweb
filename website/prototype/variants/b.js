// PROTOTYPE (#440). Variant B, "Display": the Start page's pixel display is the
// page's material. Headlines are lit on it in the accent as a dot matrix, the
// Scene runs in a bezel beside the headline and again as thin strips between
// sections, and the captures sit in the same bezels.

(function () {
  "use strict";
  var C = window.PV_CONTENT;

  PV.variants.b = {
    name: "Display",
    chrome: {
      lang: "b",
      notes: [
        "The display becomes the chrome's lit state. The selected Omnibar row, the active tab's edge and the active Space's square are lit as display cells: accent fill with the road's seams. Everything at rest stays as it is today.",
        "In QML this is one DisplayCell component: the same ShaderEffectSource, pitch and seam pattern NightRoad.qml already draws, coloured from the palette's accent. Every Omarchy theme lights it in its own colour, and opacity stays the theme's.",
        "Type stays the reader's Omarchy font. The dot-matrix lettering is the website's alone: the app cannot assume a face the theme does not name.",
      ],
    },
    mount: function (root) {
      root.innerHTML =
        '<header class="pb-top"><a class="pb-brand" href="#"></a><nav>' +
        '<a href="#pb-walk">Walk</a><a href="#pb-keys">Keyboard</a><a href="https://github.com/villekivela/omaweb#readme">Docs</a>' +
        '<a href="releases/">Releases</a><a href="https://github.com/villekivela/omaweb">GitHub</a></nav>' +
        '<span class="pb-theme"><span data-pv-theme-name></span></span></header>' +
        '<section class="pb-hero">' +
        '<div class="pb-hero__copy"><p class="pb-label">' +
        C.hero.kicker +
        '</p><h1 class="pb-lit pb-lit--xl"><span>' +
        C.hero.title.join("</span><span>") +
        '</span></h1><p class="pb-lede">' +
        C.hero.lede.join(" ") +
        '</p><p class="pb-actions"><a class="pb-btn" href="#pb-install">' +
        C.hero.cta +
        '</a><span class="pb-platform">' +
        C.hero.platform.join(" ") +
        "</span></p></div>" +
        '<div class="pb-bezel pb-bezel--hero"><div class="pb-screen"><canvas></canvas>' +
        '<p class="pb-address"><span class="pb-address__prompt"></span><span class="pb-address__text">omaweb.app</span><span class="pb-caret"></span></p></div>' +
        '<p class="pb-bezel__label"><span data-pv-scene-name></span>, lit in <span data-pv-theme-name></span></p></div>' +
        "</section>" +
        '<div class="pb-strip"><canvas></canvas></div>' +
        '<section class="pb-walk" id="pb-walk">' +
        C.steps
          .map(function (s, i) {
            return (
              '<article class="pb-step"><div class="pb-step__copy"><p class="pb-step__count"><span class="pb-lit pb-lit--num">' +
              (i + 1) +
              '</span><span class="pb-label">' +
              s.label +
              (s.key ? " <kbd>" + s.key + "</kbd>" : "") +
              "</span></p><h2>" +
              s.title +
              "</h2><p>" +
              s.body +
              '</p></div><figure class="pb-bezel"><img data-shot="' +
              s.shot +
              '" alt="' +
              s.alt +
              '" width="2720" height="1720" loading="lazy" decoding="async"></figure></article>'
            );
          })
          .join("") +
        "</section>" +
        '<div class="pb-ticker" aria-hidden="true"><div class="pb-ticker__run pb-lit">' +
        C.ticker.concat(C.ticker).join(" ▪ ") +
        "</div></div>" +
        '<section class="pb-keys" id="pb-keys"><div><p class="pb-label">Keyboard</p><h2 class="pb-h2">Learn once. Move faster forever.</h2>' +
        "<p>A small set of keys, borrowed from Vim. Every binding lives in one <code>keybindings.json</code>, and a site that needs a key can have it back.</p></div>" +
        '<ul class="pb-keypad">' +
        C.keys
          .map(function (k) {
            return (
              '<li><span class="pb-lit pb-lit--key">' +
              k[0] +
              "</span><span>" +
              k[1] +
              "</span></li>"
            );
          })
          .join("") +
        "</ul></section>" +
        '<div class="pb-strip pb-strip--low"><canvas></canvas></div>' +
        '<section class="pb-install" id="pb-install"><div><p class="pb-label">Install</p><h2 class="pb-lit pb-lit--lg"><span>' +
        C.install.title.join("</span><span>") +
        "</span></h2><p>" +
        C.install.body +
        '</p></div><div class="pb-screens"><figure class="pb-bezel pb-term"><figcaption>1 Add the repository to /etc/pacman.conf</figcaption><pre>' +
        C.install.repo +
        '</pre></figure><figure class="pb-bezel pb-term"><figcaption>2 Trust the key, then install</figcaption><pre>' +
        C.install.commands
          .map(function (c) {
            return "$ " + c;
          })
          .join("\n") +
        "</pre></figure><p>" +
        C.install.after +
        "</p></div></section>" +
        '<footer class="pb-foot"><span class="pb-foot__brand"></span><span>Web navigation system. Free and open source under MPL 2.0.</span></footer>';

      root.querySelector(".pb-brand").appendChild(PV.wordmark());
      root.querySelector(".pb-foot__brand").appendChild(PV.wordmark());
      root.querySelector(".pb-address__prompt").appendChild(PV.mark());
      root.querySelector(".pb-theme").appendChild(PV.swatches("pb-swatches"));
      root.querySelectorAll("canvas").forEach(function (canvas) {
        PV.hostScene(canvas);
      });
      function sceneLabel() {
        var name = document.querySelector(".pvbar__scene");
        root.querySelectorAll("[data-pv-scene-name]").forEach(function (n) {
          n.textContent = name ? name.textContent : "";
        });
      }
      setTimeout(sceneLabel, 0);
      document.addEventListener("click", function (e) {
        if (e.target.closest("[data-scene]")) setTimeout(sceneLabel, 0);
      });
      document.addEventListener("keydown", function (e) {
        if (e.target.closest("input, textarea") || e.ctrlKey || e.metaKey || e.altKey) return;
        if (e.key === "t" || e.key === "T") PV.stepTheme(e.shiftKey ? -1 : 1);
      });
    },
  };
})();
