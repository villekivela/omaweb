// PROTOTYPE (#440). Scene "Refined pixel": today's Start page road, cleaned
// up. Fewer, larger pixels and no seams; five tones from the theme in place
// of a dithered gradient, so the horizon is one clean step; a calmer cruise.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var BLACK = [0, 0, 0];

  function ramp(c) {
    return [
      D.mix(c.ground, BLACK, 0.55),
      D.mix(c.ground, c.glow, 0.12),
      D.mix(c.ground, c.glow, 0.45),
      c.glow,
      D.mix(c.glow, c.light, 0.55),
    ];
  }

  // Paints the refined road into a context `input.width` by `input.height`
  // display pixels, one display pixel per `input.pitch` logical pixels. Shared
  // with the CRT pixel road, which shows this picture through its glass.
  function paint(ctx, input, m) {
    var p = input.pitch;
    var g = R.geometry(input.width * p, input.height * p);
    var c = R.colours(input);
    ctx.setTransform(1 / p, 0, 0, 1 / p, 0, 0);
    R.fill(ctx, g, c, m, { soft: false });
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    var tones = ramp(c);
    var image = ctx.getImageData(0, 0, input.width, input.height);
    var data = image.data;
    for (var i = 0; i < data.length; i += 4) {
      var v = (R.grey(data, i) - 0.3) * 1.75 + 0.5;
      var t = tones[v < 0.12 ? 0 : v < 0.3 ? 1 : v < 0.55 ? 2 : v < 0.82 ? 3 : 4];
      data[i] = t[0];
      data[i + 1] = t[1];
      data[i + 2] = t[2];
      data[i + 3] = 255;
    }
    ctx.putImageData(image, 0, 0);
  }

  window.OmawebPixel = { paint: paint };

  window.OmawebScenes.register({
    id: "pixel",
    name: "Refined pixel",
    pitch: 6,
    draw: function (ctx, input) {
      paint(ctx, input, R.motion(input, 0.8));
    },
  });
})();
