// PROTOTYPE (#440). What a Scene may borrow to look like the Start page's
// display: colour mixing, and the pass that turns a drawing in the theme's
// colours into a one-colour pixel display lit in the accent. NightRoad.qml does
// the same with MultiEffect's colorization (contrast 0.6, brightness 0.28) and
// a 4 by 4 ordered dither. Optional: a Scene that wants full colour skips it.

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

  var BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];

  // Every pixel's grey, darkened by the dither, through contrast and
  // brightness, lit in `glow`. Past full brightness it runs toward white, as a
  // lit display segment does.
  function light(ctx, width, height, glow, options) {
    options = options || {};
    var contrast = options.contrast === undefined ? 0.75 : options.contrast;
    var brightness = options.brightness === undefined ? 0.2 : options.brightness;
    var image = ctx.getImageData(0, 0, width, height);
    var data = image.data;
    var gr = glow[0] / 255;
    var gg = glow[1] / 255;
    var gb = glow[2] / 255;
    for (var y = 0; y < height; y++) {
      var row = (y & 3) * 4;
      for (var x = 0; x < width; x++) {
        var i = (y * width + x) * 4;
        var grey = (data[i] * 0.299 + data[i + 1] * 0.587 + data[i + 2] * 0.114) / 255;
        grey *= 1 - (BAYER[row + (x & 3)] / 16) * 0.5;
        var v = (grey + brightness - 0.5) * (1 + contrast) + 0.5;
        if (v < 0) v = 0;
        var over = v > 1 ? Math.min(1, (v - 1) * 0.8) : 0;
        if (v > 1) v = 1;
        data[i] = (gr * v + (1 - gr) * over) * 255;
        data[i + 1] = (gg * v + (1 - gg) * over) * 255;
        data[i + 2] = (gb * v + (1 - gb) * over) * 255;
        data[i + 3] = 255;
      }
    }
    ctx.putImageData(image, 0, 0);
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

  window.OmawebDisplay = { mix: mix, css: css, light: light, layer: layer };
})();
