// PROTOTYPE (#440). Scene "Vector": the road as thin, crisp lines in the
// accent over a deep sky, drawn at the screen's own resolution. Sparse stars,
// a sun drawn as an outline, and fog that takes the far road, so the scene
// stays calm behind text.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var BLACK = [0, 0, 0];

  window.OmawebScenes.register({
    id: "vector",
    name: "Vector",
    pitch: "device",
    draw: function (ctx, input) {
      var scale = 1 / input.pitch;
      var g = R.geometry(input.width / scale, input.height / scale);
      var c = R.colours(input);
      var m = R.motion(input, 0.6);
      var accent = c.glow;
      var night = D.mix(c.ground, BLACK, 0.55);
      var fog = D.mix(c.ground, accent, 0.16);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);

      var sky = ctx.createLinearGradient(0, 0, 0, g.horizonY);
      sky.addColorStop(0, D.css(night));
      sky.addColorStop(0.7, D.css(D.mix(night, fog, 0.5)));
      sky.addColorStop(1, D.css(fog));
      ctx.fillStyle = sky;
      ctx.fillRect(0, 0, g.w, g.horizonY);

      R.stars(70, function (across, high, bright) {
        ctx.fillStyle = D.css(c.light, 0.25 + 0.5 * bright * (1 - high));
        ctx.beginPath();
        ctx.arc(across * g.w, high * g.horizonY * 0.85, bright > 0.9 ? 1.2 : 0.7, 0, Math.PI * 2);
        ctx.fill();
      });

      // The sun as an outline, cut by three hairlines below its middle.
      var r = g.h * 0.13;
      ctx.save();
      ctx.beginPath();
      ctx.rect(0, 0, g.w, g.horizonY);
      ctx.clip();
      ctx.lineWidth = 1;
      ctx.strokeStyle = D.css(accent, 0.75 + 0.25 * m.lit);
      ctx.beginPath();
      ctx.arc(g.vx, g.horizonY, r, Math.PI, 0);
      ctx.stroke();
      for (var b = 1; b <= 3; b++) {
        var y = g.horizonY - r * (0.42 - b * 0.12);
        var half = Math.sqrt(Math.max(0, r * r - (g.horizonY - y) * (g.horizonY - y)));
        ctx.beginPath();
        ctx.moveTo(g.vx - half, y);
        ctx.lineTo(g.vx + half, y);
        ctx.stroke();
      }
      ctx.restore();

      // One ridge, as a line, over a fill that hides the stars behind it.
      var pts = R.ridge(g, 4.2, 0.18, 0.2);
      ctx.beginPath();
      pts.forEach(function (p, n) {
        if (n) ctx.lineTo(p[0], p[1]);
        else ctx.moveTo(p[0], p[1]);
      });
      ctx.lineTo(g.w, g.horizonY);
      ctx.lineTo(0, g.horizonY);
      ctx.closePath();
      ctx.fillStyle = D.css(D.mix(night, fog, 0.35));
      ctx.fill();
      ctx.lineWidth = 1;
      ctx.strokeStyle = D.css(accent, 0.45);
      ctx.stroke();

      ctx.fillStyle = D.css(D.mix(night, BLACK, 0.3));
      ctx.fillRect(0, g.horizonY, g.w, g.depth);

      // The road: edges, the ground grid, the centre line and the posts.
      ctx.lineWidth = 1.25;
      ctx.strokeStyle = D.css(accent);
      ctx.beginPath();
      ctx.moveTo(g.vx, g.horizonY);
      ctx.lineTo(g.screenX(-1, 1), g.h);
      ctx.moveTo(g.vx, g.horizonY);
      ctx.lineTo(g.screenX(1, 1), g.h);
      ctx.stroke();
      R.marks(g, m.travel, m.speed, function (x, y, w, h, alpha, kind) {
        if (alpha <= 0) return;
        if (kind === "grid") {
          ctx.fillStyle = D.css(accent, 0.22 * alpha);
          ctx.fillRect(0, y, g.w, 1);
        } else if (kind === "lane") {
          ctx.fillStyle = D.css(c.light, 0.7 * alpha);
          ctx.fillRect(g.vx - 0.75, y, 1.5, h);
        } else {
          ctx.fillStyle = D.css(accent, 0.6 * alpha);
          ctx.fillRect(x + w / 2 - 0.5, y, 1, h);
        }
      });

      // Fog: the far road fades into the horizon's colour.
      var mist = ctx.createLinearGradient(0, g.horizonY, 0, g.horizonY + g.depth * 0.45);
      mist.addColorStop(0, D.css(fog, 0.95));
      mist.addColorStop(1, D.css(fog, 0));
      ctx.fillStyle = mist;
      ctx.fillRect(0, g.horizonY, g.w, g.depth * 0.45);

      var line = ctx.createLinearGradient(0, 0, g.w, 0);
      line.addColorStop(0, D.css(accent, 0));
      line.addColorStop(0.5, D.css(accent, 0.9));
      line.addColorStop(1, D.css(accent, 0));
      ctx.fillStyle = line;
      ctx.fillRect(0, g.horizonY - 0.5, g.w, 1);
    },
  });
})();
