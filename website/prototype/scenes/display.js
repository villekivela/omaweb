// PROTOTYPE (#440). What a Scene may borrow: colour mixing in the palette's
// RGB triples, and a canvas kept between frames.

(function () {
  "use strict";

  function mix(a, b, t) {
    return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
  }

  function css(c, alpha) {
    var r = Math.round(c[0]);
    var g = Math.round(c[1]);
    var b = Math.round(c[2]);
    return alpha === undefined
      ? "rgb(" + r + "," + g + "," + b + ")"
      : "rgba(" + r + "," + g + "," + b + "," + alpha + ")";
  }

  // A canvas the Scene keeps between frames, for what does not move.
  function layer(state, key, width, height) {
    var canvas = state[key];
    if (!canvas || canvas.width !== width || canvas.height !== height) {
      canvas = state[key] = document.createElement("canvas");
      canvas.width = width;
      canvas.height = height;
      canvas.fresh = true;
    } else {
      canvas.fresh = false;
    }
    return canvas;
  }

  window.OmawebDisplay = { mix: mix, css: css, layer: layer };
})();
