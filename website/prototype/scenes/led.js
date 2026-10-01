// PROTOTYPE (#440). Scene "LED matrix": the road on a coarse sign of round
// LEDs. The road is drawn small, one texel per LED, and each LED is lit to
// one of eight steps of the accent; lit ones bloom softly into their
// neighbours, as a lit sign does at night.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var BLACK = [0, 0, 0];
  var STEPS = 8;
  var CELL = 9; // logical pixels per LED

  // One sprite per step: the dot, and from the third step a bloom around it.
  function sprites(c, cell) {
    var size = cell * 3;
    var off = D.mix(c.ground, c.glow, 0.09);
    var list = [];
    for (var s = 0; s < STEPS; s++) {
      var k = s / (STEPS - 1);
      var canvas = document.createElement("canvas");
      canvas.width = canvas.height = size;
      var x = canvas.getContext("2d");
      var mid = size / 2;
      if (s >= 3) {
        var bloom = x.createRadialGradient(mid, mid, 0, mid, mid, cell * 1.4);
        bloom.addColorStop(0, D.css(c.glow, 0.14 * k * k));
        bloom.addColorStop(1, D.css(c.glow, 0));
        x.fillStyle = bloom;
        x.fillRect(0, 0, size, size);
      }
      var colour =
        s === 0 ? off : k > 0.85 ? D.mix(c.glow, c.light, (k - 0.85) * 3) : D.mix(off, c.glow, k);
      x.fillStyle = D.css(colour);
      x.beginPath();
      x.arc(mid, mid, cell * 0.36, 0, Math.PI * 2);
      x.fill();
      list.push(canvas);
    }
    return list;
  }

  window.OmawebScenes.register({
    id: "led",
    name: "LED matrix",
    pitch: "device",
    draw: function (ctx, input) {
      var scale = 1 / input.pitch;
      var cell = CELL * scale;
      var cols = Math.ceil(input.width / cell);
      var rows = Math.ceil(input.height / cell);
      var c = R.colours(input);
      var m = R.motion(input);
      var s = input.state;
      if (!s.small || s.small.width !== cols || s.small.height !== rows) {
        s.small = document.createElement("canvas");
        s.small.width = cols;
        s.small.height = rows;
        s.sprites = sprites(c, cell);
      }
      var small = s.small.getContext("2d", { willReadFrequently: true });
      var g = R.geometry(cols * CELL, rows * CELL);
      small.setTransform(1 / CELL, 0, 0, 1 / CELL, 0, 0);
      R.fill(small, g, c, m, { weight: 3.5 });
      var data = small.getImageData(0, 0, cols, rows).data;

      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.globalCompositeOperation = "source-over";
      ctx.fillStyle = D.css(D.mix(c.ground, BLACK, 0.6));
      ctx.fillRect(0, 0, input.width, input.height);
      ctx.globalCompositeOperation = "lighter";
      for (var y = 0; y < rows; y++) {
        for (var x = 0; x < cols; x++) {
          var v = (R.grey(data, (y * cols + x) * 4) - 0.36) * 1.9 + 0.5;
          var step = Math.max(0, Math.min(STEPS - 1, Math.round(v * (STEPS - 1))));
          ctx.drawImage(s.sprites[step], x * cell - cell, y * cell - cell);
        }
      }
      ctx.globalCompositeOperation = "source-over";
    },
  });
})();
