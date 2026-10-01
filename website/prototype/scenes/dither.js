// PROTOTYPE (#440). Scene "Dither": the road in one bit. Every pixel is the
// theme's ground or its accent, chosen by an 8 by 8 ordered (Bayer) matrix
// against the road's brightness, so shading is pattern and the image stays
// crisp at any size.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var BLACK = [0, 0, 0];

  // The 8 by 8 Bayer matrix, as thresholds between 0 and 1.
  var BAYER = [
    0, 32, 8, 40, 2, 34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26, 12, 44, 4, 36, 14, 46, 6, 38, 60,
    28, 52, 20, 62, 30, 54, 22, 3, 35, 11, 43, 1, 33, 9, 41, 51, 19, 59, 27, 49, 17, 57, 25, 15, 47,
    7, 39, 13, 45, 5, 37, 63, 31, 55, 23, 61, 29, 53, 21,
  ].map(function (v) {
    return (v + 0.5) / 64;
  });

  window.OmawebScenes.register({
    id: "dither",
    name: "Dither",
    pitch: 2,
    draw: function (ctx, input) {
      var p = input.pitch;
      var g = R.geometry(input.width * p, input.height * p);
      var c = R.colours(input);
      var m = R.motion(input);
      ctx.setTransform(1 / p, 0, 0, 1 / p, 0, 0);
      R.fill(ctx, g, c, m);
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      var dark = D.mix(c.ground, BLACK, 0.4);
      var lit = c.glow;
      var w = input.width;
      var image = ctx.getImageData(0, 0, w, input.height);
      var data = image.data;
      for (var y = 0; y < input.height; y++) {
        var row = (y & 7) * 8;
        for (var x = 0; x < w; x++) {
          var i = (y * w + x) * 4;
          var v = (R.grey(data, i) - 0.4) * 1.7 + 0.5;
          var t = v > BAYER[row + (x & 7)] ? lit : dark;
          data[i] = t[0];
          data[i + 1] = t[1];
          data[i + 2] = t[2];
          data[i + 3] = 255;
        }
      }
      ctx.putImageData(image, 0, 0);
    },
  });
})();
