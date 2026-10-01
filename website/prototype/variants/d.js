// PROTOTYPE (#440). Variant D, "The window": the site is an Omaweb window. The
// sidebar is the navigation: its address field is the Omnibar over the page's
// sections, Pinned holds Install, Releases, Docs and GitHub, the tab list is
// the sections, and the footer's Space squares are the themes. The page area
// opens on the Start page and reads as an editorial page below it.

(function () {
  "use strict";
  var C = window.PV_CONTENT;

  var SECTIONS = [{ id: "start-page", label: "Omaweb" }]
    .concat(
      C.steps.map(function (s) {
        return { id: s.id, label: s.label };
      }),
    )
    .concat([
      { id: "keys", label: "Keyboard" },
      { id: "install", label: "Install" },
    ]);

  PV.variants.d = {
    name: "The window",
    chrome: {
      lang: "d",
      notes: [
        "The chrome is the website's navigation, so the chrome itself does not change.",
        "The browser's own pages take the website's editorial header instead: History, Agent activity, each Settings section and a What's new page open on a large heading in the reader's Omarchy font at display size, a one-line lede and a rule.",
        "In QML that is a PageHeader component on the kit's Style sizes and the palette's text and muted roles, so it follows the theme and the reader's font like everything else.",
      ],
    },
    mount: function (root) {
      root.innerHTML =
        '<div class="pd-window">' +
        '<aside class="pd-side" aria-label="Site">' +
        '<div class="pd-tools"><button type="button" class="pd-collapse" aria-label="Hide sidebar (Ctrl+B)" title="Hide sidebar  Ctrl+B"></button><span></span><a class="pd-brand" href="#"></a></div>' +
        '<label class="pd-address"><span class="pd-prompt"></span><input type="text" placeholder="Search this page" aria-label="Search this page" autocomplete="off" spellcheck="false"><kbd>o</kbd></label>' +
        '<nav class="pd-pinned" aria-label="Pinned">' +
        '<a href="#pd-install"><b>↓</b>Install</a><a href="releases/"><b>R</b>Releases</a>' +
        '<a href="https://github.com/villekivela/omaweb#readme"><b>D</b>Docs</a><a href="https://github.com/villekivela/omaweb"><b>G</b>GitHub</a></nav>' +
        '<ol class="pd-tabs">' +
        SECTIONS.map(function (s, i) {
          return (
            '<li><a href="#pd-' +
            s.id +
            '" data-section="' +
            s.id +
            '"' +
            (i === 0 ? ' aria-current="true"' : "") +
            "><i></i>" +
            s.label +
            "</a></li>"
          );
        }).join("") +
        "</ol>" +
        '<footer class="pd-foot"><span class="pd-foot__label">Theme</span><div class="pd-spaces"></div><span class="pd-keys"><kbd>J</kbd><kbd>K</kbd></span></footer>' +
        "</aside>" +
        '<main class="pd-page">' +
        '<section class="pd-start" id="pd-start-page"><div class="pd-scene"><canvas></canvas></div>' +
        '<div class="pd-start__copy"><p class="pd-kicker">' +
        C.hero.kicker +
        '</p><h1 class="pd-title">' +
        C.hero.title.join("<br>") +
        '</h1><p class="pd-lede">' +
        C.hero.lede.join("<br>") +
        '</p><a class="pd-btn" href="#pd-install">' +
        C.hero.cta +
        "</a></div></section>" +
        C.steps
          .map(function (s) {
            return (
              '<section class="pd-section" id="pd-' +
              s.id +
              '"><header><p class="pd-kicker">' +
              s.label +
              (s.key ? " <kbd>" + s.key + "</kbd>" : "") +
              '</p><h2 class="pd-h2">' +
              s.title +
              '</h2><p class="pd-lede">' +
              s.body +
              '</p></header><figure class="pd-shot"><img data-shot="' +
              s.shot +
              '" alt="' +
              s.alt +
              '" width="2720" height="1720" loading="lazy" decoding="async"></figure></section>'
            );
          })
          .join("") +
        '<section class="pd-section" id="pd-keys"><header><p class="pd-kicker">Keyboard</p><h2 class="pd-h2">Learn once. Move faster forever.</h2>' +
        '<p class="pd-lede">This page answers them too: <kbd>J</kbd> and <kbd>K</kbd> move between its sections, <kbd>o</kbd> searches it, <kbd>Ctrl</kbd>+<kbd>B</kbd> hides the sidebar.</p></header><dl class="pd-keylist">' +
        C.keys
          .map(function (k) {
            return "<div><dt><kbd>" + k[0] + "</kbd></dt><dd>" + k[1] + "</dd></div>";
          })
          .join("") +
        "</dl></section>" +
        '<section class="pd-section" id="pd-install"><header><p class="pd-kicker">Install</p><h2 class="pd-h2">' +
        C.install.title.join(" ") +
        '</h2><p class="pd-lede">' +
        C.install.body +
        '</p></header><ol class="pd-install"><li><p>Add the repository to <code>/etc/pacman.conf</code></p><pre>' +
        C.install.repo +
        "</pre></li><li><p>Trust the key, then install</p><pre>" +
        C.install.commands
          .map(function (c) {
            return "$ " + c;
          })
          .join("\n") +
        "</pre></li></ol><p>" +
        C.install.after +
        "</p></section>" +
        '<footer class="pd-pagefoot">Web navigation system. Free and open source under MPL 2.0.</footer>' +
        "</main></div>";

      root.querySelector(".pd-brand").appendChild(PV.wordmark());
      root.querySelector(".pd-prompt").appendChild(PV.mark());
      PV.hostScene(root.querySelector(".pd-scene canvas"));

      // The themes as Space squares: each one its theme's accent on its ground.
      var spaces = root.querySelector(".pd-spaces");
      C.themes.forEach(function (t) {
        var b = PV.el("button", "pd-space", t[1].charAt(0));
        b.type = "button";
        b.dataset.theme = t[0];
        b.dataset.pvThemeChoice = t[0];
        b.title = t[1];
        b.setAttribute("aria-label", t[1]);
        b.addEventListener("click", function () {
          PV.setTheme(t[0]);
        });
        spaces.appendChild(b);
      });

      var tabs = [].slice.call(root.querySelectorAll(".pd-tabs a"));
      var sections = SECTIONS.map(function (s) {
        return root.querySelector("#pd-" + s.id);
      });
      var current = 0;
      function mark(i) {
        current = i;
        tabs.forEach(function (t, n) {
          if (n === i) t.setAttribute("aria-current", "true");
          else t.removeAttribute("aria-current");
        });
      }
      var seen = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (e) {
            if (e.isIntersecting) mark(sections.indexOf(e.target));
          });
        },
        { rootMargin: "-40% 0px -55% 0px" },
      );
      sections.forEach(function (s) {
        seen.observe(s);
      });

      var input = root.querySelector(".pd-address input");
      input.addEventListener("input", function () {
        var q = input.value.trim().toLowerCase();
        tabs.forEach(function (t) {
          t.parentElement.hidden = q && t.textContent.toLowerCase().indexOf(q) < 0;
        });
      });
      input.addEventListener("keydown", function (e) {
        if (e.key === "Enter") {
          var first = tabs.filter(function (t) {
            return !t.parentElement.hidden;
          })[0];
          if (first) first.click();
          input.value = "";
          input.dispatchEvent(new Event("input"));
          input.blur();
        } else if (e.key === "Escape") {
          input.value = "";
          input.dispatchEvent(new Event("input"));
          input.blur();
        }
      });

      var windowNode = root.querySelector(".pd-window");
      function toggle() {
        windowNode.classList.toggle("is-collapsed");
      }
      root.querySelector(".pd-collapse").addEventListener("click", toggle);
      document.addEventListener("keydown", function (e) {
        if (e.ctrlKey && (e.key === "b" || e.key === "B")) {
          e.preventDefault();
          toggle();
          return;
        }
        if (e.target.closest("input, textarea") || e.ctrlKey || e.metaKey || e.altKey) return;
        if (e.key === "o") {
          e.preventDefault();
          input.focus();
        } else if (e.key === "J" || e.key === "K") {
          var next = Math.max(0, Math.min(sections.length - 1, current + (e.key === "J" ? 1 : -1)));
          sections[next].scrollIntoView();
          mark(next);
        } else if (e.key === "t" || e.key === "T") {
          PV.stepTheme(1);
        }
      });
    },
  };
})();
