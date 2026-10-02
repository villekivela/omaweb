// What a fixture page tells the recording about itself, so a beat can be checked by what the page
// saw rather than by a picture: where it is, how wide its viewport is, and how many of its ad
// slots are drawn. It reports when it loads and again whenever the viewport changes size.
(() => {
  const report = () => {
    const ads = [...document.querySelectorAll(".ad")].filter((slot) => slot.offsetHeight > 0);
    const body = JSON.stringify({
      host: location.host,
      path: location.pathname,
      width: innerWidth,
      height: innerHeight,
      adsShown: ads.length,
      adCreatives: document.querySelectorAll(".adsprout").length,
    });
    fetch("/beacon", { method: "POST", body, keepalive: true });
  };
  let pending = 0;
  addEventListener("resize", () => {
    clearTimeout(pending);
    pending = setTimeout(report, 250);
  });
  report();
})();
