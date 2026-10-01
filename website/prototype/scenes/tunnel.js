// PROTOTYPE (#440). A second Scene, to prove the contract carries more than one
// drawing: a night tunnel, its ceiling lamps streaming overhead and the way
// out lit at the far end. Drawn in the palette and lit as the same display as
// the road, so the two sit together in every theme.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var BLACK = [0, 0, 0];
  var WHITE = [255, 255, 255];

  function wrap(value, span) {
    return ((value % span) + span) % span;
  }

  // The tunnel's cross-section at depth z: a box with chamfered upper corners,
  // centred on the vanishing point, shrinking as 1/z.
  function section(g, z) {
    var hw = g.hw / z;
    var top = g.vy - g.up / z;
    var bottom = g.vy + g.down / z;
    var cut = hw * 0.35;
    return [
      [g.vx - hw, bottom],
      [g.vx - hw, top + cut],
      [g.vx - hw + cut, top],
      [g.vx + hw - cut, top],
      [g.vx + hw, top + cut],
      [g.vx + hw, bottom],
    ];
  }

  function path(ctx, points, close) {
    ctx.beginPath();
    points.forEach(function (pt, i) {
      if (i === 0) ctx.moveTo(pt[0], pt[1]);
      else ctx.lineTo(pt[0], pt[1]);
    });
    if (close) ctx.closePath();
  }

  window.OmawebScenes.register({
    id: "tunnel",
    name: "Tunnel",
    pitch: 4,
    seams: true,
    draw: function (ctx, input) {
      var p = input.pitch;
      var w = input.width * p;
      var h = input.height * p;
      var pal = input.palette;
      var ground = input.dark ? pal.ground : D.mix(pal.text, BLACK, 0.45);
      var light = input.dark ? pal.text : pal.ground;
      var glow = pal.accent;
      var deep = D.mix(ground, BLACK, 0.5);
      var g = {
        vx: w / 2,
        vy: h * 0.52,
        hw: w * 0.62,
        up: h * 0.62,
        down: h * 0.5,
      };
      var travel = input.reducedMotion ? 0.4 : input.time * 2.2;

      ctx.setTransform(1 / p, 0, 0, 1 / p, 0, 0);
      ctx.fillStyle = D.css(deep);
      ctx.fillRect(0, 0, w, h);

      // The way out: the night beyond the far portal, glowing.
      var far = section(g, 7);
      var out = ctx.createRadialGradient(g.vx, g.vy, 0, g.vx, g.vy, w * 0.35);
      out.addColorStop(0, D.css(D.mix(light, WHITE, 0.5)));
      out.addColorStop(0.08, D.css(D.mix(glow, light, 0.4)));
      out.addColorStop(0.3, D.css(glow, 0.6));
      out.addColorStop(1, D.css(glow, 0));
      ctx.fillStyle = out;
      ctx.fillRect(0, 0, w, h);
      path(ctx, far, true);
      ctx.fillStyle = D.css(D.mix(glow, light, 0.55));
      ctx.fill();

      // Walls, ceiling and road as four faces between the near and far
      // sections, each shaded toward the light at the end.
      var near = section(g, 0.9);
      var faces = [
        [0, 1, 0.55],
        [1, 2, 0.4],
        [2, 3, 0.3],
        [3, 4, 0.4],
        [4, 5, 0.55],
      ];
      faces.forEach(function (face) {
        var a = face[0];
        var b = face[1];
        var shade = ctx.createLinearGradient(
          (near[a][0] + near[b][0]) / 2,
          (near[a][1] + near[b][1]) / 2,
          (far[a][0] + far[b][0]) / 2,
          (far[a][1] + far[b][1]) / 2,
        );
        shade.addColorStop(0, D.css(D.mix(ground, light, 0.08 * face[2])));
        shade.addColorStop(0.6, D.css(D.mix(ground, glow, 0.25 * face[2] + 0.1)));
        shade.addColorStop(1, D.css(D.mix(ground, glow, 0.7)));
        path(ctx, [near[a], near[b], far[b], far[a]], true);
        ctx.fillStyle = shade;
        ctx.fill();
      });
      var road = ctx.createLinearGradient(0, h, 0, g.vy);
      road.addColorStop(0, D.css(D.mix(deep, BLACK, 0.5)));
      road.addColorStop(1, D.css(D.mix(ground, glow, 0.35)));
      path(ctx, [near[0], near[5], far[5], far[0]], true);
      ctx.fillStyle = road;
      ctx.fill();

      // Rings: the tunnel's ribs, passing.
      for (var r = 0; r < 14; r++) {
        var z = 0.9 + wrap(r * 0.5 - travel, 7);
        ctx.globalAlpha = Math.min(1, (z - 0.9) * 2) * (1 - z / 7.5) * 0.8;
        path(ctx, section(g, z), false);
        ctx.strokeStyle = D.css(D.mix(ground, light, 0.5));
        ctx.lineWidth = Math.max(1, 6 / z);
        ctx.stroke();
      }

      // Lamps on both upper chamfers, streaming overhead.
      for (var l = 0; l < 40; l++) {
        var side = l % 2 === 0 ? -1 : 1;
        var zl = 0.9 + wrap(Math.floor(l / 2) * 0.35 - travel, 7);
        var len = 0.1;
        var s0 = section(g, zl);
        var s1 = section(g, zl + len);
        var at = side < 0 ? [s0[1], s0[2], s1[1], s1[2]] : [s0[4], s0[3], s1[4], s1[3]];
        var mx = function (a, b) {
          return [a[0] * 0.55 + b[0] * 0.45, a[1] * 0.55 + b[1] * 0.45];
        };
        var n = mx(at[0], at[1]);
        var f = mx(at[2], at[3]);
        ctx.globalAlpha = Math.min(1, (zl - 0.9) * 3) * Math.max(0, Math.min(1, 1.1 - zl / 7));
        ctx.strokeStyle = D.css(D.mix(light, WHITE, 0.4));
        ctx.lineWidth = Math.max(1.5, 22 / zl);
        ctx.lineCap = "round";
        ctx.beginPath();
        ctx.moveTo(n[0], n[1]);
        ctx.lineTo(f[0], f[1]);
        ctx.stroke();
      }

      // The centre line.
      for (var m = 0; m < 20; m++) {
        var zm = 0.9 + wrap(m * 0.35 - travel, 7);
        var y0 = g.vy + g.down / zm;
        var y1 = g.vy + g.down / (zm + 0.3);
        var mw = Math.max(1, 14 / zm);
        ctx.globalAlpha = Math.max(0, Math.min(1, (zm - 0.9) * 3) * (1 - zm / 7.5));
        ctx.fillStyle = D.css(D.mix(glow, WHITE, 0.3));
        ctx.fillRect(g.vx - mw / 2, y1, mw, Math.max(1, y0 - y1));
      }
      ctx.globalAlpha = 1;

      var v = ctx.createRadialGradient(g.vx, g.vy, 0, g.vx, g.vy, Math.max(w, h) * 0.8);
      v.addColorStop(0.35, "rgba(0,0,0,0)");
      v.addColorStop(1, "rgba(0,0,0,0.65)");
      ctx.fillStyle = v;
      ctx.fillRect(0, 0, w, h);

      ctx.setTransform(1, 0, 0, 1, 0, 0);
      D.light(ctx, input.width, input.height, glow);
    },
  });
})();
