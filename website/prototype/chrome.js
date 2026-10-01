// PROTOTYPE (#440). A mock of the browser's chrome, the sidebar, its footer and
// the Omnibar over the Start page, drawn in one variant's language so the
// drawer can show what that variant asks of the app. Content is the lab's
// seeded day: the same tabs, Spaces and query the captures show. Not the app:
// the real chrome stays QML on Omarchy's kit.

(function () {
  "use strict";

  var TABS = [
    ["New tab", true],
    ["Generate the website’s screenshots", false],
    ["Qt Quick Scene Graph", false],
    ["Night drive: a photo essay", false],
    ["xdg-shell protocol", false],
    ["Arch Linux - qt6-webengine", false],
    ["Omarchy", false],
  ];

  // kind, title, detail, Space letter for another Space's tab
  // As the lab's capture lists them for "qt": tabs here, another Space's tab,
  // history, then the engine's suggestions.
  var ROWS = [
    ["tab", "Qt Quick Scene Graph", "doc.qt.io", ""],
    ["tab", "Arch Linux - qt6-webengine", "archlinux.org", ""],
    ["space", "QML Applications", "doc.qt.io", "W"],
    ["history", "Arch Linux - qt6-webengine", "archlinux.org", ""],
    ["suggest", "qt quick shapes", "", ""],
    ["suggest", "qt 6.11 release notes", "", ""],
  ];

  var KIND = { tab: "switch tab", space: "switch tab", history: "open", suggest: "search" };

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;");
  }

  window.PV.chrome = function (lang) {
    var root = PV.el("div", "cm cm--" + lang);
    var tabs = TABS.map(function (t, i) {
      return (
        '<li class="cm-tab' +
        (t[1] ? " is-active" : "") +
        '"><i class="cm-fav cm-fav--' +
        (i % 4) +
        '"></i><span>' +
        esc(t[0]) +
        "</span></li>"
      );
    }).join("");
    var rows = ROWS.map(function (r, i) {
      return (
        '<li class="cm-row cm-row--' +
        r[0] +
        (i === 0 ? " is-selected" : "") +
        '">' +
        (r[3]
          ? '<i class="cm-space cm-space--w">' + r[3] + "</i>"
          : '<i class="cm-glyph cm-glyph--' + r[0] + '"></i>') +
        '<span class="cm-row__title">' +
        esc(r[1]) +
        "</span>" +
        (r[2] ? '<span class="cm-row__detail">' + esc(r[2]) + "</span>" : "") +
        (KIND[r[0]] ? '<span class="cm-row__kind">' + KIND[r[0]] + "</span>" : "") +
        "</li>"
      );
    }).join("");
    root.innerHTML =
      '<div class="cm-window">' +
      '<aside class="cm-side">' +
      '<div class="cm-tools"><i></i><i></i><span></span><i></i><i></i></div>' +
      '<div class="cm-address">New tab</div>' +
      '<div class="cm-pinned"><i class="cm-fav--0"></i><i class="cm-fav--1"></i><i class="cm-fav--2"></i><i class="cm-fav--3"></i></div>' +
      '<ul class="cm-tabs">' +
      tabs +
      "</ul>" +
      '<footer class="cm-foot">' +
      '<i class="cm-space cm-space--p is-active">P</i><i class="cm-space cm-space--w">W</i>' +
      '<i class="cm-space cm-space--agent" title="Agent Space">' +
      '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 3h6M12 3v3M5 9a3 3 0 0 1 3-3h8a3 3 0 0 1 3 3v8a3 3 0 0 1-3 3H8a3 3 0 0 1-3-3zM9.5 12.5h.01M14.5 12.5h.01M9 16h6M2.5 12v3M21.5 12v3"/></svg>' +
      "</i>" +
      '<span></span><i class="cm-icon"></i><i class="cm-icon"></i>' +
      "</footer>" +
      "</aside>" +
      '<div class="cm-page">' +
      '<div class="cm-scene"><canvas></canvas></div>' +
      '<div class="cm-omnibar">' +
      '<div class="cm-field"><span class="cm-prompt"></span><span class="cm-typed">qt</span><span class="cm-caret"></span><span class="cm-scope">This tab</span></div>' +
      '<ul class="cm-rows">' +
      rows +
      "</ul>" +
      '<div class="cm-hints"><span>↑↓ select</span><span>⏎ open</span><span>esc close</span></div>' +
      "</div>" +
      "</div>" +
      "</div>";
    root.querySelector(".cm-prompt").appendChild(PV.mark());
    // The Start page's road under the Omnibar, as the app draws it.
    requestAnimationFrame(function () {
      PV.hostScene(root.querySelector(".cm-scene canvas"));
    });
    return root;
  };
})();
