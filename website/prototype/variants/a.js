// PROTOTYPE (#440). Variant A, "Start page": the site opens as the browser's
// own Start page. The road fills the first screen, the Omnibar rests on its
// horizon, and its rows are the page's index: type to filter, arrows to pick,
// Return to go. The walk below keeps the Omnibar's list pinned beside it.

(function () {
  "use strict";
  var C = window.PV_CONTENT;

  function row(step, i) {
    return (
      '<li class="pa-row" role="option" id="pa-opt-' +
      step.id +
      '" data-step="' +
      step.id +
      '" aria-selected="' +
      (i === 0) +
      '"><span class="pa-row__label">' +
      step.label +
      '</span><span class="pa-row__title">' +
      step.title +
      "</span>" +
      (step.key ? "<kbd>" + step.key + "</kbd>" : "") +
      "</li>"
    );
  }

  PV.variants.a = {
    name: "Start page",
    chrome: {
      lang: "app",
      notes: [
        "The chrome is the reference: nothing in the app changes. The website takes its look from the browser instead.",
        "What the site borrows: the Omnibar's row (label, title, quiet detail, key at the right), the accent rule on the selected row, the key-hint line, and the mark as the field's prompt.",
        "The Scene is shared: the site runs the same contract the Start page would, so a reader's own Scene could show on both.",
      ],
    },
    mount: function (root) {
      var steps = C.steps;
      root.innerHTML =
        '<header class="pa-top"><a class="pa-brand" href="#"></a><nav>' +
        '<a href="#pa-features">Features</a><a href="#pa-keyboard">Keyboard</a>' +
        '<a href="https://github.com/villekivela/omaweb#readme">Docs</a><a href="releases/">Releases</a>' +
        '<a href="https://github.com/villekivela/omaweb">GitHub</a></nav></header>' +
        '<section class="pa-start">' +
        '<div class="pa-scene"><canvas></canvas></div>' +
        '<div class="pa-sky"><p class="pa-kicker">' +
        C.hero.kicker +
        '</p><h1 class="pa-title">' +
        C.hero.title.join("<br>") +
        "</h1></div>" +
        '<div class="pa-omnibar" role="search">' +
        '<label class="pa-field"><span class="pa-prompt"></span>' +
        '<input type="text" role="combobox" aria-expanded="true" aria-controls="pa-list" aria-label="Find on this page" placeholder="Where to? Try spaces, or press ↓" autocomplete="off" spellcheck="false">' +
        '<span class="pa-scope">This page</span></label>' +
        '<ul class="pa-list" id="pa-list" role="listbox">' +
        steps.map(row).join("") +
        "</ul>" +
        '<div class="pa-hints"><span>↑↓ select</span><span>⏎ open</span><span class="pa-theme">Theme <kbd>T</kbd> <b data-pv-theme-name></b></span></div>' +
        "</div>" +
        '<div class="pa-below"><a class="pa-install" href="#pa-install">' +
        C.hero.cta +
        '</a><p class="pa-lede">' +
        C.hero.lede.join("<br>") +
        "</p></div>" +
        "</section>" +
        '<section class="pa-walk" id="pa-walk">' +
        '<div class="pa-walk__index"><p class="pa-kicker">The walk</p><ol class="pa-list pa-list--walk">' +
        steps
          .map(function (s, i) {
            return (
              '<li class="pa-row" data-step="' +
              s.id +
              '" aria-current="' +
              (i === 0 ? "step" : "false") +
              '"><a href="#pa-' +
              s.id +
              '"><span class="pa-row__label">' +
              s.label +
              "</span>" +
              (s.key ? "<kbd>" + s.key + "</kbd>" : "") +
              "</a></li>"
            );
          })
          .join("") +
        "</ol></div>" +
        '<div class="pa-walk__steps">' +
        steps
          .map(function (s) {
            return (
              '<article class="pa-step" id="pa-' +
              s.id +
              '" data-step="' +
              s.id +
              '"><figure class="pa-shot"><img data-shot="' +
              s.shot +
              '" alt="' +
              s.alt +
              '" width="2720" height="1720" loading="lazy" decoding="async"></figure>' +
              "<h2>" +
              s.title +
              "</h2><p>" +
              s.body +
              "</p></article>"
            );
          })
          .join("") +
        "</div></section>" +
        '<section class="pa-features" id="pa-features"><h2 class="pa-h2">What it does</h2><dl>' +
        C.features
          .map(function (f) {
            return "<div><dt>" + f[0] + "</dt><dd>" + f[1] + "</dd></div>";
          })
          .join("") +
        "</dl></section>" +
        '<section class="pa-keyboard" id="pa-keyboard"><h2 class="pa-h2">Learn once.<br>Move faster forever.</h2>' +
        "<p>A small set of keys, borrowed from Vim, and every one of them yours to rebind in <code>keybindings.json</code>.</p><ul>" +
        C.keys
          .map(function (k) {
            return "<li><kbd>" + k[0] + "</kbd><span>" + k[1] + "</span></li>";
          })
          .join("") +
        "</ul></section>" +
        '<section class="pa-install-section" id="pa-install"><h2 class="pa-h2">' +
        C.install.title.join("<br>") +
        "</h2><p>" +
        C.install.body +
        '</p><div class="pa-panel"><div class="pa-panel__head"><span>1 Add the repository</span><span>/etc/pacman.conf</span></div><pre>' +
        C.install.repo +
        '</pre></div><div class="pa-panel"><div class="pa-panel__head"><span>2 Trust the key, then install</span><span>terminal</span></div><pre>' +
        C.install.commands
          .map(function (c) {
            return '<span class="pa-dollar">$ </span>' + c;
          })
          .join("\n") +
        '</pre></div><p class="pa-after">' +
        C.install.after +
        "</p></section>" +
        '<footer class="pa-foot"><span class="pa-foot__brand"></span><span>Web navigation system. Free and open source under MPL 2.0.</span></footer>';

      root.querySelector(".pa-brand").appendChild(PV.wordmark());
      root.querySelector(".pa-foot__brand").appendChild(PV.wordmark());
      root.querySelector(".pa-prompt").appendChild(PV.mark());
      root.querySelector(".pa-hints").appendChild(PV.swatches("pa-swatches"));
      PV.hostScene(root.querySelector(".pa-scene canvas"));

      // The Omnibar: filter, pick, go.
      var input = root.querySelector(".pa-field input");
      var options = [].slice.call(root.querySelectorAll("#pa-list .pa-row"));
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
      function open(option) {
        var target = root.querySelector("#pa-" + option.dataset.step);
        target.scrollIntoView({
          behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth",
        });
      }
      input.addEventListener("input", function () {
        var q = input.value.trim().toLowerCase();
        options.forEach(function (o) {
          o.hidden = q && o.textContent.toLowerCase().indexOf(q) < 0;
        });
        select(0);
      });
      input.addEventListener("keydown", function (e) {
        if (e.key === "ArrowDown") {
          select(selected + 1);
          e.preventDefault();
        } else if (e.key === "ArrowUp") {
          select(selected - 1);
          e.preventDefault();
        } else if (e.key === "Enter") {
          var list = shown();
          if (list.length) open(list[selected]);
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
          open(o);
        });
      });

      // The pinned list follows the step in view.
      var walkRows = [].slice.call(root.querySelectorAll(".pa-list--walk .pa-row"));
      var seen = new IntersectionObserver(
        function (entries) {
          entries.forEach(function (e) {
            if (!e.isIntersecting) return;
            walkRows.forEach(function (r) {
              r.setAttribute(
                "aria-current",
                r.dataset.step === e.target.dataset.step ? "step" : "false",
              );
            });
          });
        },
        { rootMargin: "-45% 0px -45% 0px" },
      );
      root.querySelectorAll(".pa-step").forEach(function (s) {
        seen.observe(s);
      });

      // o focuses the Omnibar and T steps the theme, as in the browser.
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
