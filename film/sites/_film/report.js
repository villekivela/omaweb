// What a fixture page tells the recording about itself, so a beat can be checked by what the page
// saw rather than by a picture: where it is, how wide its viewport is, how many of its ad slots
// are drawn and how many link hints Omaweb has put over it. It reports when it loads, whenever the
// viewport changes size, and whenever the link hints come up or go.
(() => {
  // Omaweb's link hints are one overlay that names its count (keyboard-navigation.js).
  const hints = () => {
    const overlay = document.getElementById("__omaweb_link_hints");
    return overlay ? parseInt(overlay.getAttribute("aria-label"), 10) || 0 : 0;
  };
  const report = () => {
    const ads = [...document.querySelectorAll(".ad")].filter((slot) => slot.offsetHeight > 0);
    const body = JSON.stringify({
      host: location.host,
      path: location.pathname,
      width: innerWidth,
      height: innerHeight,
      adsShown: ads.length,
      adCreatives: document.querySelectorAll(".adsprout").length,
      hints: hints(),
    });
    fetch("/beacon", { method: "POST", body, keepalive: true });
  };
  let pending = 0;
  addEventListener("resize", () => {
    clearTimeout(pending);
    pending = setTimeout(report, 250);
  });
  let shown = 0;
  new MutationObserver(() => {
    if (hints() === shown) return;
    shown = hints();
    report();
  }).observe(document.documentElement, { childList: true, subtree: true });
  report();
})();
